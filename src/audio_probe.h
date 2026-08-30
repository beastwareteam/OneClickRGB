#pragma once

//=============================================================================
// audio_probe.h - Tonausgabe mit Loopback-Gegenprobe (Phase 6.1)
//
// Wozu das da ist
// ---------------
// Die JVC Bassrolle haengt an einem reinen Cinch/Klinke-Eingang. Es gibt also
// keine Steuerleitung, ueber die man sie schalten koennte - der einzige denkbare
// Weg ist die "Signal-Sense"-Automatik vieler aktiver Subwoofer: liegt Pegel am
// Eingang an, schaltet sich das Geraet ein. Ob diese Rolle das kann, weiss
// niemand. Diese Datei ist deshalb ein MESSGERAET und keine Steuerung.
//
// Warum ueberhaupt eine Gegenprobe
// --------------------------------
// CLAUDE.md Regel 1 verlangt fuer jeden Schreibvorgang auf Hardware einen
// Read-back: "Keyboard set (Mode: 0x05)" nach einem ungeprueften Write ist
// verboten. Fuer Audio gilt dasselbe, nur ist die Falle groesser: ein
// AUDCLNT-Aufruf, der S_OK zurueckgibt, beweist, dass Windows die Daten
// entgegengenommen hat - nicht, dass am Klinkenstecker etwas anliegt. Ein
// stummgeschalteter Endpunkt, eine Lautstaerke auf 0, ein falsch gewaehltes
// Geraet: alle drei liefern S_OK und Stille. Ohne Gegenprobe waere das Ergebnis
// "die Rolle reagiert nicht" - und die Ursache waere unser eigener Fehler
// gewesen, nicht die Rolle.
//
// Deshalb nimmt jeder Lauf ueber AUDCLNT_STREAMFLAGS_LOOPBACK auf demselben
// Endpunkt auf, was tatsaechlich hinausgeht, und misst daraus Pegel (RMS) und
// Frequenz (Goertzel). Erst wenn beides zum Sollwert passt, heisst der Ton
// "verifiziert".
//
// Warum Shared-Mode und nicht Exclusive
// -------------------------------------
// WASAPI-Loopback funktioniert nicht, solange der Endpunkt im Exclusive-Mode
// belegt ist - der Mixer, an dem Loopback abgreift, laeuft dann gar nicht.
// Exclusive-Mode gaebe bit-genaue Ausgabe, aber ohne jeden Beleg, dass sie
// stattgefunden hat. Fuer ein Messgeraet ist das der falsche Tausch: bit-genau
// und unbelegt ist weniger wert als ein paar Promille Mixer-Abweichung mit
// Beleg. Also Shared-Mode.
//
// Grenzen, die hier hart im Code stehen (Phase 6.3)
// -------------------------------------------------
//   * kMaxDbfs   = -12 dBFS. Nach oben nicht konfigurierbar.
//   * kMinFreqHz =  40 Hz. Darunter und bei Gleichanteil heizt die Schwingspule,
//     ohne dass man es hoert - bei einer Bassrolle ist genau das die Gefahr,
//     weil sie dieses Band verstaerkt.
//   * Ein ueberschrittener Wert wird begrenzt UND gemeldet (levelClamped /
//     freqClamped), nie stillschweigend zurechtgebogen. Eine Sonde, die die
//     Anfrage aendert und die Antwort auf die alte Frage meldet, misst nichts.
//
// Diese Datei ist rein additiv: sie beruehrt weder die Apply-Queue noch die
// Energie-Ausloeser noch RGBConfig.
//=============================================================================

#include <windows.h>
#include <mmreg.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

// Die beiden Subformat-GUIDs von WAVEFORMATEXTENSIBLE.
//
// Sie werden hier aus dem dokumentierten Bildungsgesetz zusammengesetzt statt
// aus <ksmedia.h> geholt: dessen Symbole verlangen ein zusaetzliches Linkziel,
// und das Bildungsgesetz ist stabil und nachpruefbar. Gegengelesen an
// shared/ksmedia.h:738 des Windows SDK 10.0.26100.0:
//
//     #define DEFINE_WAVEFORMATEX_GUID(x) \
//         (USHORT)(x), 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
//
// mit WAVE_FORMAT_PCM = 1 (mmreg.h:2418) und WAVE_FORMAT_IEEE_FLOAT = 3.
//
// Ausgeschrieben, weil eine von Hand getippte GUID in genau diesem Projekt
// schon einmal ein Byte verloren hat (Data4 mit sieben statt acht Eintraegen in
// der Display-Power-Registrierung). Deshalb hier acht Eintraege, gezaehlt.
#ifndef ONECLICK_WAVEFORMATEX_SUBGUID
#define ONECLICK_WAVEFORMATEX_SUBGUID(x) \
    { (unsigned long)(x), 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } }
#endif

namespace audioprobe {

//-----------------------------------------------------------------------------
// Grenzen und Konstanten
//-----------------------------------------------------------------------------

struct Limits {
    static constexpr double kMaxDbfs   = -12.0;    // Obergrenze, hart
    static constexpr double kMinFreqHz =  40.0;    // Untergrenze, hart
    static constexpr double kMaxFreqHz = 20000.0;
    static constexpr double kFloorDbfs = -200.0;   // "nichts gemessen"
    static constexpr double kSilenceDbfs = -80.0;  // Schwelle der Stilleprobe
    static constexpr int    kMaxHoldMs = 30000;
    static constexpr int    kSettleMs  = 300;      // Anlaufzeit, wird verworfen
};

static const double kPi = 3.14159265358979323846;

static const GUID kSubFormatPcm   = ONECLICK_WAVEFORMATEX_SUBGUID(WAVE_FORMAT_PCM);
static const GUID kSubFormatFloat = ONECLICK_WAVEFORMATEX_SUBGUID(WAVE_FORMAT_IEEE_FLOAT);

//-----------------------------------------------------------------------------
// Datentypen
//-----------------------------------------------------------------------------

struct Endpoint {
    std::wstring id;
    std::wstring name;
    bool         isDefault = false;
};

struct ToneRequest {
    double       freqHz = 1000.0;
    double       dbfs   = -30.0;
    int          holdMs = 5000;
    std::wstring endpointId;       // leer = Standard-Wiedergabegeraet
    bool         silent = false;   // true = Stilleprobe (nichts ausgeben)
};

struct ToneResult {
    bool   attempted    = false;
    bool   verified     = false;

    double askedFreqHz  = 0.0;
    double usedFreqHz   = 0.0;
    double askedDbfs    = 0.0;
    double usedDbfs     = 0.0;
    bool   freqClamped  = false;
    bool   levelClamped = false;

    double measuredDbfs   = Limits::kFloorDbfs;
    double measuredFreqHz = 0.0;

    unsigned sampleRate     = 0;
    unsigned channels       = 0;
    unsigned framesAnalyzed = 0;

    std::wstring endpointName;
    std::string  error;            // leer = kein Fehler
};

//-----------------------------------------------------------------------------
// Kleine Helfer
//-----------------------------------------------------------------------------

inline double AmplitudeFromDbfs(double dbfs) {
    return std::pow(10.0, dbfs / 20.0);
}

inline double DbfsFromAmplitude(double a) {
    if (a <= 1e-12) return Limits::kFloorDbfs;
    return 20.0 * std::log10(a);
}

// Goertzel: Betrag einer einzelnen Frequenz, normiert auf Amplitude (0..1).
// Billiger als eine FFT und genau das, was hier gebraucht wird.
inline double Goertzel(const std::vector<double>& x, double freqHz, double rate) {
    const size_t n = x.size();
    if (n < 8 || rate <= 0.0) return 0.0;

    const double w     = 2.0 * kPi * freqHz / rate;
    const double cw    = std::cos(w);
    const double sw    = std::sin(w);
    const double coeff = 2.0 * cw;

    double s0 = 0.0, s1 = 0.0, s2 = 0.0;
    for (size_t i = 0; i < n; ++i) {
        s0 = x[i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double re = s1 - s2 * cw;
    const double im = s2 * sw;
    return std::sqrt(re * re + im * im) * 2.0 / (double)n;
}

// Dominante Frequenz durch logarithmischen Goertzel-Scan.
//
// Warum gescannt und nicht einfach bei der Sollfrequenz nachgesehen: "wir haben
// 1 kHz zurueckgelesen" darf nicht heissen "wir haben bei 1 kHz nachgesehen und
// dort etwas gefunden". Laeuft in Wahrheit ein anderer Ton durch den Mixer,
// muss die Messung das zeigen koennen, statt ihn auf die erwartete Frequenz
// zu projizieren.
inline double DominantFrequency(const std::vector<double>& x, double rate,
                                double loHz, double hiHz, int bins = 400) {
    if (x.size() < 64 || rate <= 0.0 || bins < 2) return 0.0;
    if (hiHz > rate * 0.45) hiHz = rate * 0.45;
    if (loHz < 1.0)         loHz = 1.0;
    if (hiHz <= loHz)       return 0.0;

    const double lr   = std::log(loHz);
    const double step = (std::log(hiHz) - lr) / (double)(bins - 1);

    double bestF = 0.0, bestM = -1.0;
    for (int i = 0; i < bins; ++i) {
        const double f = std::exp(lr + step * (double)i);
        const double m = Goertzel(x, f, rate);
        if (m > bestM) { bestM = m; bestF = f; }
    }
    if (bestF <= 0.0) return 0.0;

    // Nachscharfstellen im Fenster einer Rasterbreite um den groben Gipfel.
    //
    // Ohne diesen Schritt ist die gemeldete Frequenz auf die Rasterweite genau
    // und nicht besser: bei 600 Bins zwischen 20 Hz und 4 kHz sind das 0,88 %
    // je Schritt. Gemessen wurden so fuer einen exakten 1000-Hz-Ton 997,6 Hz -
    // richtig innerhalb des Rasters, aber nur 0,12 Prozentpunkte von der
    // 1-%-Toleranz entfernt, an der die Selbstprobe entscheidet. Eine Sonde,
    // deren Bestehen davon abhaengt, wo zufaellig ein Rasterpunkt liegt, misst
    // ihr eigenes Raster mit. Die feine Suche macht das Ergebnis
    // rasterunabhaengig.
    const double lo2 = bestF * std::exp(-step);
    const double hi2 = bestF * std::exp( step);
    const double fstep = (hi2 - lo2) / 64.0;
    for (int i = 0; i <= 64; ++i) {
        const double f = lo2 + fstep * (double)i;
        if (f <= 0.0 || f >= rate * 0.5) continue;
        const double m = Goertzel(x, f, rate);
        if (m > bestM) { bestM = m; bestF = f; }
    }
    return bestF;
}

// Effektivwert, ausgedrueckt als Amplitude in dBFS. Ein Sinus mit Amplitude A
// hat RMS A/sqrt(2); zurueckgerechnet wird auf A, damit der Messwert direkt mit
// dem angeforderten dBFS vergleichbar ist.
inline double RmsDbfs(const std::vector<double>& x) {
    if (x.empty()) return Limits::kFloorDbfs;
    double acc = 0.0;
    for (size_t i = 0; i < x.size(); ++i) acc += x[i] * x[i];
    const double rms = std::sqrt(acc / (double)x.size());
    return DbfsFromAmplitude(rms * std::sqrt(2.0));
}

//-----------------------------------------------------------------------------
// COM-Handhabung
//-----------------------------------------------------------------------------

// Initialisiert COM fuer diesen Thread und gibt es im Destruktor wieder frei.
// RPC_E_CHANGED_MODE heisst "der Thread ist schon anders initialisiert" - dann
// duerfen wir benutzen, aber nicht aufraeumen.
class ComScope {
public:
    ComScope() {
        const HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        owned_ = SUCCEEDED(hr);
        ok_    = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    }
    ~ComScope() { if (owned_) CoUninitialize(); }
    bool ok() const { return ok_; }
private:
    bool owned_ = false;
    bool ok_    = false;
};

template <class T>
inline void SafeRelease(T*& p) { if (p) { p->Release(); p = NULL; } }

inline std::string HrText(const char* what, HRESULT hr) {
    char buf[160];
    snprintf(buf, sizeof(buf), "%s failed (hr=0x%08lX)", what, (unsigned long)hr);
    return std::string(buf);
}

//-----------------------------------------------------------------------------
// Endpunkte auflisten
//-----------------------------------------------------------------------------

inline bool ListRenderEndpoints(std::vector<Endpoint>& out, std::string& err) {
    out.clear();
    err.clear();

    ComScope com;
    if (!com.ok()) { err = "CoInitializeEx failed"; return false; }

    IMMDeviceEnumerator* enumr = NULL;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), (void**)&enumr);
    if (FAILED(hr)) { err = HrText("CoCreateInstance(MMDeviceEnumerator)", hr); return false; }

    std::wstring defaultId;
    IMMDevice* def = NULL;
    if (SUCCEEDED(enumr->GetDefaultAudioEndpoint(eRender, eConsole, &def)) && def) {
        LPWSTR id = NULL;
        if (SUCCEEDED(def->GetId(&id)) && id) { defaultId = id; CoTaskMemFree(id); }
        SafeRelease(def);
    }

    IMMDeviceCollection* coll = NULL;
    hr = enumr->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &coll);
    if (FAILED(hr)) {
        SafeRelease(enumr);
        err = HrText("EnumAudioEndpoints", hr);
        return false;
    }

    UINT count = 0;
    coll->GetCount(&count);
    for (UINT i = 0; i < count; ++i) {
        IMMDevice* dev = NULL;
        if (FAILED(coll->Item(i, &dev)) || !dev) continue;

        Endpoint e;
        LPWSTR id = NULL;
        if (SUCCEEDED(dev->GetId(&id)) && id) { e.id = id; CoTaskMemFree(id); }

        IPropertyStore* props = NULL;
        if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props)) && props) {
            PROPVARIANT pv;
            PropVariantInit(&pv);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &pv)) &&
                pv.vt == VT_LPWSTR && pv.pwszVal) {
                e.name = pv.pwszVal;
            }
            PropVariantClear(&pv);
            SafeRelease(props);
        }
        e.isDefault = (!defaultId.empty() && e.id == defaultId);
        out.push_back(e);
        SafeRelease(dev);
    }

    SafeRelease(coll);
    SafeRelease(enumr);
    return true;
}

//-----------------------------------------------------------------------------
// Ein Ton, ausgegeben und im selben Lauf zurueckgemessen
//-----------------------------------------------------------------------------

inline bool RunTone(const ToneRequest& req, ToneResult& res) {
    res = ToneResult();
    res.askedFreqHz = req.freqHz;
    res.askedDbfs   = req.dbfs;

    // --- Grenzen anwenden und das Begrenzen MELDEN (Phase 6.3) --------------
    double freq = req.freqHz;
    if (freq < Limits::kMinFreqHz) { freq = Limits::kMinFreqHz; res.freqClamped = true; }
    if (freq > Limits::kMaxFreqHz) { freq = Limits::kMaxFreqHz; res.freqClamped = true; }

    double dbfs = req.dbfs;
    if (dbfs > Limits::kMaxDbfs) { dbfs = Limits::kMaxDbfs; res.levelClamped = true; }

    int hold = req.holdMs;
    if (hold < 200)                hold = 200;
    if (hold > Limits::kMaxHoldMs) hold = Limits::kMaxHoldMs;

    res.usedFreqHz = freq;
    res.usedDbfs   = req.silent ? Limits::kFloorDbfs : dbfs;

    ComScope com;
    if (!com.ok()) { res.error = "CoInitializeEx failed"; return false; }

    IMMDeviceEnumerator* enumr = NULL;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), (void**)&enumr);
    if (FAILED(hr)) { res.error = HrText("CoCreateInstance(MMDeviceEnumerator)", hr); return false; }

    IMMDevice* dev = NULL;
    if (req.endpointId.empty())
        hr = enumr->GetDefaultAudioEndpoint(eRender, eConsole, &dev);
    else
        hr = enumr->GetDevice(req.endpointId.c_str(), &dev);
    if (FAILED(hr) || !dev) {
        SafeRelease(enumr);
        res.error = HrText("GetDefaultAudioEndpoint/GetDevice", hr);
        return false;
    }

    {   // Endpunktname mitfuehren - ein Messwert ohne Geraetenamen ist wertlos
        IPropertyStore* props = NULL;
        if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props)) && props) {
            PROPVARIANT pv;
            PropVariantInit(&pv);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &pv)) &&
                pv.vt == VT_LPWSTR && pv.pwszVal) {
                res.endpointName = pv.pwszVal;
            }
            PropVariantClear(&pv);
            SafeRelease(props);
        }
    }

    IAudioClient*        rc  = NULL;
    IAudioRenderClient*  rr  = NULL;
    IAudioClient*        cc  = NULL;
    IAudioCaptureClient* cr  = NULL;
    WAVEFORMATEX*        mix = NULL;
    bool started    = false;
    bool capStarted = false;

    // Ein einziger Aufraeumpfad. Jeder frueh zurueckkehrende Zweig unten ruft
    // ihn auf - sonst bleibt bei einem Fehlschlag ein laufender Stream stehen
    // und der naechste Lauf misst dessen Reste.
    struct Cleanup {
        IAudioClient** rc; IAudioRenderClient** rr;
        IAudioClient** cc; IAudioCaptureClient** cr;
        WAVEFORMATEX** mix; IMMDevice** dev; IMMDeviceEnumerator** en;
        bool* started; bool* capStarted;
        void run() {
            if (*started    && *rc) (*rc)->Stop();
            if (*capStarted && *cc) (*cc)->Stop();
            *started = false; *capStarted = false;
            SafeRelease(*cr); SafeRelease(*cc);
            SafeRelease(*rr); SafeRelease(*rc);
            if (*mix) { CoTaskMemFree(*mix); *mix = NULL; }
            SafeRelease(*dev); SafeRelease(*en);
        }
    } cleanup{ &rc, &rr, &cc, &cr, &mix, &dev, &enumr, &started, &capStarted };

    hr = dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&rc);
    if (FAILED(hr)) { res.error = HrText("Activate(IAudioClient/render)", hr); cleanup.run(); return false; }

    hr = rc->GetMixFormat(&mix);
    if (FAILED(hr) || !mix) { res.error = HrText("GetMixFormat", hr); cleanup.run(); return false; }

    // Nur die beiden Formate, die der Windows-Mixer tatsaechlich liefert.
    // Alles andere wird abgelehnt statt falsch interpretiert - eine Sonde, die
    // ein unbekanntes Sampleformat "irgendwie" fuellt, misst ihren eigenen Fehler.
    bool isFloat = false;
    if (mix->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        isFloat = true;
    } else if (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const WAVEFORMATEXTENSIBLE* we = (const WAVEFORMATEXTENSIBLE*)mix;
        if (IsEqualGUID(we->SubFormat, kSubFormatFloat))    isFloat = true;
        else if (IsEqualGUID(we->SubFormat, kSubFormatPcm)) isFloat = false;
        else { res.error = "unsupported mix subformat"; cleanup.run(); return false; }
    } else if (mix->wFormatTag != WAVE_FORMAT_PCM) {
        res.error = "unsupported mix format"; cleanup.run(); return false;
    }
    if (!isFloat && mix->wBitsPerSample != 16) {
        res.error = "unsupported PCM bit depth"; cleanup.run(); return false;
    }

    res.sampleRate = mix->nSamplesPerSec;
    res.channels   = mix->nChannels;

    const REFERENCE_TIME bufDur = 2000000;   // 200 ms
    hr = rc->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufDur, 0, mix, NULL);
    if (FAILED(hr)) {
        res.error = HrText("IAudioClient::Initialize(render)", hr);
        if (hr == AUDCLNT_E_DEVICE_IN_USE)
            res.error += " - Endpunkt im Exclusive-Mode belegt; Loopback ist dann unmoeglich";
        cleanup.run(); return false;
    }

    UINT32 bufFrames = 0;
    hr = rc->GetBufferSize(&bufFrames);
    if (FAILED(hr) || bufFrames == 0) { res.error = HrText("GetBufferSize", hr); cleanup.run(); return false; }

    hr = rc->GetService(__uuidof(IAudioRenderClient), (void**)&rr);
    if (FAILED(hr)) { res.error = HrText("GetService(IAudioRenderClient)", hr); cleanup.run(); return false; }

    // --- Loopback-Client auf DEMSELBEN Endpunkt ------------------------------
    hr = dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&cc);
    if (FAILED(hr)) { res.error = HrText("Activate(IAudioClient/loopback)", hr); cleanup.run(); return false; }

    hr = cc->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
                        bufDur, 0, mix, NULL);
    if (FAILED(hr)) { res.error = HrText("IAudioClient::Initialize(loopback)", hr); cleanup.run(); return false; }

    hr = cc->GetService(__uuidof(IAudioCaptureClient), (void**)&cr);
    if (FAILED(hr)) { res.error = HrText("GetService(IAudioCaptureClient)", hr); cleanup.run(); return false; }

    // --- Tonerzeugung --------------------------------------------------------
    const double amp   = req.silent ? 0.0 : AmplitudeFromDbfs(dbfs);
    const double rate  = (double)mix->nSamplesPerSec;
    const double phInc = 2.0 * kPi * freq / rate;
    const WORD   ch    = mix->nChannels;
    double       phase = 0.0;

    // Der Sinus wird ueber alle Aufrufe hinweg phasenstetig fortgeschrieben.
    // Bei jedem Puffer wieder bei 0 zu beginnen erzeugt an jeder Puffergrenze
    // einen Sprung - der ist breitbandig und wuerde die Frequenzmessung
    // verfaelschen, die er eigentlich belegen soll.
    struct Filler {
        bool isFloat; double amp; double phInc; WORD ch; double* phase;
        void operator()(BYTE* dst, UINT32 frames) const {
            if (isFloat) {
                float* f = (float*)dst;
                for (UINT32 i = 0; i < frames; ++i) {
                    const float v = (float)(amp * std::sin(*phase));
                    *phase += phInc;
                    if (*phase > 2.0 * kPi) *phase -= 2.0 * kPi;
                    for (WORD c = 0; c < ch; ++c) f[i * ch + c] = v;
                }
            } else {
                int16_t* s = (int16_t*)dst;
                for (UINT32 i = 0; i < frames; ++i) {
                    const double d = amp * std::sin(*phase);
                    *phase += phInc;
                    if (*phase > 2.0 * kPi) *phase -= 2.0 * kPi;
                    long q = std::lround(d * 32767.0);
                    if (q >  32767) q =  32767;
                    if (q < -32768) q = -32768;
                    for (WORD c = 0; c < ch; ++c) s[i * ch + c] = (int16_t)q;
                }
            }
        }
    } fill{ isFloat, amp, phInc, ch, &phase };

    // Puffer einmal vorfuellen, damit beim Start kein Loch entsteht.
    {
        BYTE* data = NULL;
        if (SUCCEEDED(rr->GetBuffer(bufFrames, &data)) && data) {
            fill(data, bufFrames);
            rr->ReleaseBuffer(bufFrames, 0);
        }
    }

    res.attempted = true;

    hr = rc->Start();
    if (FAILED(hr)) { res.error = HrText("IAudioClient::Start(render)", hr); cleanup.run(); return false; }
    started = true;

    hr = cc->Start();
    if (FAILED(hr)) { res.error = HrText("IAudioClient::Start(loopback)", hr); cleanup.run(); return false; }
    capStarted = true;

    // --- Ausgeben und gleichzeitig mitschneiden ------------------------------
    std::vector<double> mono;
    mono.reserve((size_t)(rate * (double)hold / 1000.0) + 1024);

    const ULONGLONG t0          = GetTickCount64();
    const ULONGLONG settleUntil = t0 + (ULONGLONG)Limits::kSettleMs;

    while (GetTickCount64() - t0 < (ULONGLONG)hold) {
        Sleep(5);

        UINT32 pad = 0;
        if (SUCCEEDED(rc->GetCurrentPadding(&pad))) {
            const UINT32 avail = (bufFrames > pad) ? (bufFrames - pad) : 0;
            if (avail) {
                BYTE* data = NULL;
                if (SUCCEEDED(rr->GetBuffer(avail, &data)) && data) {
                    fill(data, avail);
                    rr->ReleaseBuffer(avail, 0);
                }
            }
        }

        // Loopback leeren. Wird nicht gelesen, laeuft der Puffer ueber und die
        // spaetere Messung haette Luecken, die wie Verzerrung aussehen.
        UINT32 packet = 0;
        while (SUCCEEDED(cr->GetNextPacketSize(&packet)) && packet > 0) {
            BYTE*  cdata   = NULL;
            UINT32 cframes = 0;
            DWORD  cflags  = 0;
            if (FAILED(cr->GetBuffer(&cdata, &cframes, &cflags, NULL, NULL))) break;

            const bool keep = GetTickCount64() >= settleUntil;
            if (keep && cframes) {
                const bool silentFlag = (cflags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
                for (UINT32 i = 0; i < cframes; ++i) {
                    double v = 0.0;
                    if (!silentFlag && cdata) {
                        if (isFloat) v = (double)((const float*)cdata)[i * ch];
                        else         v = (double)((const int16_t*)cdata)[i * ch] / 32768.0;
                    }
                    mono.push_back(v);
                }
            }
            cr->ReleaseBuffer(cframes);
            packet = 0;
        }
    }

    cc->Stop(); capStarted = false;
    rc->Stop(); started    = false;

    // --- Auswertung ----------------------------------------------------------
    res.framesAnalyzed = (unsigned)mono.size();

    if (mono.size() < 512) {
        res.error = "loopback returned too few frames to measure";
        cleanup.run();
        return false;
    }

    res.measuredDbfs = RmsDbfs(mono);

    if (req.silent) {
        // Bei der Stilleprobe gibt es keine Sollfrequenz. "Verifiziert" heisst
        // hier ausschliesslich: es ist wirklich still.
        res.measuredFreqHz = 0.0;
        res.verified = (res.measuredDbfs < Limits::kSilenceDbfs);
        cleanup.run();
        return true;
    }

    const double scanHi = (freq * 4.0 < rate * 0.45) ? freq * 4.0 : rate * 0.45;
    res.measuredFreqHz  = DominantFrequency(mono, rate, Limits::kMinFreqHz * 0.5, scanHi, 600);

    const double freqErr  = (freq > 0.0) ? std::fabs(res.measuredFreqHz - freq) / freq : 1.0;
    const double levelErr = std::fabs(res.measuredDbfs - dbfs);

    // Toleranzen: 1 % Frequenz, 1 dB Pegel. Beides muss halten - ein richtiger
    // Pegel bei falscher Frequenz ist kein bestandener Test, sondern ein Hinweis
    // darauf, dass etwas anderes durch den Mixer laeuft.
    res.verified = (freqErr <= 0.01) && (levelErr <= 1.0);

    cleanup.run();
    return true;
}

} // namespace audioprobe
