#pragma once

//=============================================================================
// power_manager.h - Systemlage und Geraeteinventar (Phase 4)
//
// Diese Datei SCHALTET NICHTS. Sie liest, bewertet und meldet. Das Schalten
// kommt in Phase 5 und bekommt seinen eigenen Rueckweg.
//
// Die Frage, die sie beantwortet
// ------------------------------
// "Welche Geraete sind dauerhaft bestromt, obwohl sie nie belastet werden?" -
// ausdruecklich nicht auf USB beschraenkt, sondern gerade auch interne
// Controller.
//
// Was daran messbar ist und was nicht
// -----------------------------------
// Es gibt KEINE Windows-API, die sagt, ob ein Geraet je benutzt wurde. Was es
// gibt, sind vier voneinander unabhaengige Angaben:
//
//   1. D-Zustand      - steht das Geraet in D0 (voll bestromt)?
//   2. Kindgeraete    - haengt ueberhaupt etwas daran?
//   3. Treiberdienst  - laedt ein Treiber, oder ist der Knoten leer?
//   4. Lastzaehler    - klassenabhaengig, und nur fuer manche Klassen vorhanden
//
// Sie werden GETRENNT gefuehrt und nie ineinander aufgerundet. Ein Geraet ohne
// Klassenzaehler bekommt UNBEKANNT, nicht "unbenutzt" (globale Konvention 4).
// Die Entscheidung trifft am Ende ein Mensch; diese Datei schlaegt vor.
//
// Die Sicherheitsampel
// --------------------
// Sie stuetzt sich zuerst auf eine echte Systemangabe und nicht auf eine
// Namensliste: CM_Get_DevNode_Status liefert DN_DISABLEABLE, und wo Windows
// selbst sagt "nicht abschaltbar", wird gar nichts erst angeboten. Erst danach
// kommen Klassen-Ausschluesse und die gemessene Last.
//
// Auf dieser Maschine gemessen (2026-08-30): 212 praesente Geraete, davon 169
// in D0. Der Netzwerkzaehler trennte nachweislich richtig - OpenVPN-Adapter
// 0,00 MB in 15,2 h, Intel I225-V 1154 MB empfangen und damit gesperrt.
// (Eine PowerShell-Vorabmessung zaehlte 204 statt 212, weil sie zusaetzlich auf
// Status=OK filterte. Beide Zahlen sind richtig, sie zaehlen Verschiedenes.)
//
// Der wichtigste Befund dieses Laufs ist ein negativer, und er steht so auch im
// Bericht: KEIN Geraet war gleichzeitig "frei" und in D0. Was als frei
// eingestuft wird, hat Windows meist schon selbst nach D3 geschickt. Die
// gesuchte Sorte - dauerhaft bestromt UND ungenutzt UND gefahrlos abschaltbar -
// ist die Schnittmenge dreier Bedingungen, und die ist oft leer.
//=============================================================================

// Reihenfolge ist hier nicht Geschmackssache: netioapi.h (MIB_IF_ROW2,
// GetIfTable2) baut auf den Adress-Typen aus ws2def.h/ws2ipdef.h auf, und die
// kommen nur ueber winsock2.h herein. Ohne diese beiden Zeilen meldet der
// Compiler MIB_IF_TABLE2 als undeklariert. Der Hauptquelltext definiert
// WIN32_LEAN_AND_MEAN, windows.h zieht also kein winsock.h nach - sonst waere
// die klassische Winsock-1/2-Kollision faellig.
#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <powerbase.h>
#include <iphlpapi.h>
#include <netioapi.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")
#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace powermgr {

//-----------------------------------------------------------------------------
// Systemlage
//-----------------------------------------------------------------------------

// Beantwortet, WELCHE Energiezustaende diese Maschine ueberhaupt kann. Ohne das
// laesst sich nicht sagen, warum ein Ausloeser feuert oder ausbleibt - und
// genau diese Trennung war der Ausgangspunkt des ganzen Vorgangs.
struct SystemInfo {
    bool valid   = false;    // false => UNBEKANNT, nicht "kann nichts"
    bool s1      = false;
    bool s2      = false;
    bool s3      = false;
    bool s4      = false;    // Ruhezustand vom System unterstuetzt
    bool s5      = false;
    bool hiberFilePresent = false;   // Ruhezustand tatsaechlich eingerichtet
    bool aoac    = false;    // Modern Standby (S0 low power idle)
    bool batteryPresent = false;
};

inline bool QuerySystemInfo(SystemInfo& out) {
    out = SystemInfo();

    SYSTEM_POWER_CAPABILITIES spc = {};
    // LONG statt NTSTATUS: NTSTATUS ist genau das, kommt aber je nach
    // Header-Reihenfolge aus winternl.h/ntdef.h, und winternl.h hier
    // hereinzuziehen holt sich Kollisionen mit windows.h ins Haus.
    const LONG st = CallNtPowerInformation(SystemPowerCapabilities,
                                           NULL, 0, &spc, sizeof(spc));
    if (st != 0) return false;   // Quelle fehlt -> UNBEKANNT

    out.valid            = true;
    out.s1               = spc.SystemS1  != FALSE;
    out.s2               = spc.SystemS2  != FALSE;
    out.s3               = spc.SystemS3  != FALSE;
    out.s4               = spc.SystemS4  != FALSE;
    out.s5               = spc.SystemS5  != FALSE;
    out.hiberFilePresent = spc.HiberFilePresent != FALSE;
    out.aoac             = spc.AoAc      != FALSE;
    out.batteryPresent   = spc.SystemBatteriesPresent != FALSE;
    return true;
}

//-----------------------------------------------------------------------------
// Geraeteinventar
//-----------------------------------------------------------------------------

enum Safety {
    SAFE_BLOCKED = 0,   // wird gar nicht erst angeboten
    SAFE_CAUTION = 1,   // moeglich, aber mit Folgen
    SAFE_FREE    = 2    // abschaltbar, in D0, ohne Kinder, ohne gemessene Last
};

inline const wchar_t* SafetyName(Safety s) {
    switch (s) {
        case SAFE_FREE:    return L"frei";
        case SAFE_CAUTION: return L"achtung";
        default:           return L"gesperrt";
    }
}

struct DeviceEntry {
    std::wstring instanceId;
    std::wstring description;
    std::wstring cls;
    std::wstring service;

    ULONG status  = 0;
    ULONG problem = 0;
    bool  disableable    = false;
    bool  rootEnumerated = false;
    bool  disabled       = false;

    // D0 = 1 ... D3 = 4 (DEVICE_POWER_STATE). 0 = nicht lesbar.
    int   powerState = 0;
    // -1 = nicht ermittelt
    int   childCount = -1;

    // Klassenspezifische Last. -1 heisst UNBEKANNT (keine Zaehler fuer diese
    // Klasse) und nicht 0.
    long long rxBytes = -1;
    long long txBytes = -1;
    bool  mediaConnected = false;

    Safety       safety = SAFE_BLOCKED;
    std::wstring reason;
};

// Klassen, die nie angeboten werden.
//
// SoftwareComponent und SoftwareDevice stehen bewusst darin: das ist keine
// Hardware, die Strom zieht, sondern ein Treiberknoten. Ihr "D0" bedeutet
// nichts, und sie abzuschalten spart kein Milliwatt - sie waeren nur Rauschen
// in einer Liste, die Vertrauen verdienen muss.
inline bool IsBlockedClass(const std::wstring& c) {
    static const wchar_t* kBlocked[] = {
        L"System", L"Processor", L"Computer", L"Volume", L"DiskDrive",
        L"Display", L"Firmware", L"Keyboard", L"Mouse", L"HIDClass",
        L"USB", L"HDC", L"SecurityDevices", L"VolumeSnapshot",
        L"SoftwareComponent", L"SoftwareDevice",
        // Beide aus dem ersten Lauf dieser Sonde nachgetragen, weil sie dort
        // faelschlich als FREI erschienen:
        //   Monitor               - "Generic Monitor (PL2795)". Ein Bildschirm
        //                           ist kein ungenutzter Controller, und wer
        //                           ihn abschaltet, sucht den Fehler danach
        //                           ueberall, nur nicht in dieser Liste.
        //   AudioProcessingObject - "Voice Clarity". Ein Software-Effekt, der
        //                           nichts bestromt.
        L"Monitor", L"AudioProcessingObject"
    };
    for (size_t i = 0; i < sizeof(kBlocked) / sizeof(kBlocked[0]); ++i)
        if (c == kBlocked[i]) return true;
    return false;
}

// Unsere eigenen RGB-Geraete, erkannt an der Hersteller-ID in der Instanz-ID.
//
// Der Anlass ist ein Befund aus dem ersten Lauf dieser Sonde: der
// "AURA LED Controller" (VID_0B05, PID_19AF) stand auf FREI. Er laeuft unter
// der Klasse USBDevice mit dem WinUSB-Treiber, nicht unter HIDClass - die
// Klassenliste allein hat ihn also durchgelassen. Ein Energiemanager, der das
// Geraet abschaltet, dessen Beleuchtung er steuert, saegt den Ast ab, auf dem
// er sitzt; danach meldet die App "nicht gefunden", und niemand verbindet das
// mehr mit einem Haken in einer Geraeteliste.
//
// Deshalb wird hier nach Hersteller-ID gesperrt und nicht nach Klassennamen.
// Die IDs sind dieselben, die die Adapter zum Oeffnen benutzen (namespace
// Devices im Hauptquelltext): ASUS 0x0B05, SteelSeries 0x1038, EVision 0x3299.
inline bool IsOwnRgbDevice(const std::wstring& instanceId) {
    static const wchar_t* kOwnVids[] = {
        L"VID_0B05",   // ASUS Aura
        L"VID_1038",   // SteelSeries Rival 600
        L"VID_3299"    // EVision Keyboard / Edge
    };
    // Instanz-IDs stehen in Grossbuchstaben, aber darauf verlassen wir uns
    // nicht: einmal hochziehen kostet nichts.
    std::wstring up = instanceId;
    for (size_t i = 0; i < up.size(); ++i)
        if (up[i] >= L'a' && up[i] <= L'z') up[i] = (wchar_t)(up[i] - L'a' + L'A');

    for (size_t i = 0; i < sizeof(kOwnVids) / sizeof(kOwnVids[0]); ++i)
        if (up.find(kOwnVids[i]) != std::wstring::npos) return true;
    return false;
}

inline bool IsCautionClass(const std::wstring& c) {
    static const wchar_t* kCaution[] = { L"Net", L"SCSIAdapter", L"MEDIA", L"AudioEndpoint" };
    for (size_t i = 0; i < sizeof(kCaution) / sizeof(kCaution[0]); ++i)
        if (c == kCaution[i]) return true;
    return false;
}

namespace detail {

inline std::wstring RegProp(HDEVINFO set, SP_DEVINFO_DATA& dev, DWORD prop) {
    DWORD needed = 0;
    SetupDiGetDeviceRegistryPropertyW(set, &dev, prop, NULL, NULL, 0, &needed);
    if (needed == 0) return std::wstring();
    std::vector<BYTE> buf(needed + sizeof(wchar_t), 0);
    if (!SetupDiGetDeviceRegistryPropertyW(set, &dev, prop, NULL,
                                           buf.data(), needed, NULL))
        return std::wstring();
    return std::wstring((const wchar_t*)buf.data());
}

// Zaehlt die Kindknoten. Ein Controller ohne Kinder ist ein Controller, an dem
// nichts haengt - der staerkste klassenunabhaengige Hinweis auf "unbenutzt".
inline int CountChildren(DEVINST inst) {
    DEVINST child = 0;
    if (CM_Get_Child(&child, inst, 0) != CR_SUCCESS) return 0;
    int n = 1;
    DEVINST sib = 0;
    while (CM_Get_Sibling(&sib, child, 0) == CR_SUCCESS) {
        n++;
        child = sib;
        if (n > 4096) break;   // Schutz gegen einen kaputten Baum
    }
    return n;
}

// Aktueller Geraete-Energiezustand aus CM_POWER_DATA.
inline int CurrentPowerState(DEVINST inst) {
    CM_POWER_DATA pd = {};
    ULONG len = sizeof(pd);
    ULONG type = 0;
    if (CM_Get_DevNode_Registry_PropertyW(inst, CM_DRP_DEVICE_POWER_DATA,
                                          &type, &pd, &len, 0) != CR_SUCCESS)
        return 0;
    if (pd.PD_Size < sizeof(CM_POWER_DATA)) return 0;
    return (int)pd.PD_MostRecentPowerState;
}

// Netzwerkzaehler, ueber die Beschreibung zugeordnet.
//
// MIB_IF_ROW2::Description traegt denselben Text wie die Geraetebeschreibung
// aus SetupAPI - nachgemessen mit Get-NetAdapter/Get-PnpDevice. Ueber den
// Namen zu gehen ist unschoen, aber die Alternative (Interface-GUID aus dem
// Devnode) ist nicht fuer alle Adaptertypen vorhanden.
struct NetRow {
    std::wstring description;
    long long    rx = 0;
    long long    tx = 0;
    bool         connected = false;
};

inline void CollectNetRows(std::vector<NetRow>& out) {
    out.clear();
    MIB_IF_TABLE2* table = NULL;
    if (GetIfTable2(&table) != NO_ERROR || !table) return;
    for (ULONG i = 0; i < table->NumEntries; ++i) {
        const MIB_IF_ROW2& r = table->Table[i];
        NetRow n;
        n.description = r.Description;
        n.rx          = (long long)r.InOctets;
        n.tx          = (long long)r.OutOctets;
        n.connected   = (r.MediaConnectState == MediaConnectStateConnected);
        out.push_back(n);
    }
    FreeMibTable(table);
}

} // namespace detail

// Liest alle praesenten Geraete und bewertet sie. Liefert false, wenn die
// Quelle selbst fehlt - dann ist das Ergebnis UNBEKANNT und nicht "leer".
inline bool Inventory(std::vector<DeviceEntry>& out, std::string& err) {
    out.clear();
    err.clear();

    std::vector<detail::NetRow> netRows;
    detail::CollectNetRows(netRows);

    HDEVINFO set = SetupDiGetClassDevsW(NULL, NULL, NULL,
                                        DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (set == INVALID_HANDLE_VALUE) {
        err = "SetupDiGetClassDevs failed";
        return false;
    }

    SP_DEVINFO_DATA dev = {};
    dev.cbSize = sizeof(dev);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &dev); ++i) {
        DeviceEntry e;

        wchar_t idBuf[MAX_DEVICE_ID_LEN] = {};
        if (CM_Get_Device_IDW(dev.DevInst, idBuf, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS)
            e.instanceId = idBuf;

        e.description = detail::RegProp(set, dev, SPDRP_FRIENDLYNAME);
        if (e.description.empty())
            e.description = detail::RegProp(set, dev, SPDRP_DEVICEDESC);
        e.cls     = detail::RegProp(set, dev, SPDRP_CLASS);
        e.service = detail::RegProp(set, dev, SPDRP_SERVICE);

        ULONG status = 0, problem = 0;
        if (CM_Get_DevNode_Status(&status, &problem, dev.DevInst, 0) == CR_SUCCESS) {
            e.status         = status;
            e.problem        = problem;
            e.disableable    = (status & DN_DISABLEABLE)    != 0;
            e.rootEnumerated = (status & DN_ROOT_ENUMERATED) != 0;
            e.disabled       = ((status & DN_HAS_PROBLEM) != 0) &&
                               (problem == CM_PROB_DISABLED);
        }

        e.powerState = detail::CurrentPowerState(dev.DevInst);
        e.childCount = detail::CountChildren(dev.DevInst);

        // Klassenspezifische Last: bisher nur Netzwerk. Alles andere behaelt
        // -1 = UNBEKANNT, statt sich als "keine Last" auszugeben.
        if (e.cls == L"Net" && !e.description.empty()) {
            for (size_t k = 0; k < netRows.size(); ++k) {
                if (netRows[k].description == e.description) {
                    e.rxBytes        = netRows[k].rx;
                    e.txBytes        = netRows[k].tx;
                    e.mediaConnected = netRows[k].connected;
                    break;
                }
            }
        }

        // --- Ampel ---------------------------------------------------------
        if (IsOwnRgbDevice(e.instanceId)) {
            // Zuerst, noch vor DN_DISABLEABLE: unsere eigenen Geraete sind
            // abschaltbar, und genau deshalb muessen sie hier zuerst raus.
            e.safety = SAFE_BLOCKED;
            e.reason = L"eigenes RGB-Geraet - wird von dieser App gesteuert";
        } else if (!e.disableable) {
            e.safety = SAFE_BLOCKED;
            e.reason = L"Windows meldet das Geraet als nicht abschaltbar";
        } else if (IsBlockedClass(e.cls)) {
            e.safety = SAFE_BLOCKED;
            e.reason = L"Klasse ausgeschlossen (Kern- oder eigenes Geraet)";
        } else if (e.rxBytes > 0 || e.txBytes > 0 || e.mediaConnected) {
            // Gemessene Last schlaegt jeden Haken. Der Intel I225-V dieser
            // Maschine faellt hierunter - er hatte 1154 MB empfangen.
            e.safety = SAFE_BLOCKED;
            e.reason = L"in Benutzung (Zaehler oder Verbindung belegen Last)";
        } else if (e.rootEnumerated || e.childCount > 0 || IsCautionClass(e.cls)) {
            e.safety = SAFE_CAUTION;
            e.reason = (e.childCount > 0)
                         ? L"Controller mit angeschlossenen Geraeten"
                         : L"Klasse oder Enumeration verlangt Vorsicht";
        } else {
            e.safety = SAFE_FREE;
            e.reason = L"abschaltbar, ohne Kindgeraete, ohne gemessene Last";
        }

        out.push_back(e);
    }

    SetupDiDestroyDeviceInfoList(set);
    return true;
}

//-----------------------------------------------------------------------------
// Bericht
//-----------------------------------------------------------------------------

// Wide -> UTF-8.
//
// Der Bericht wird als Bytestrom geschrieben. Gibt man wchar_t direkt ueber
// %ls aus, landet alles ausserhalb von ASCII als Fragezeichen in der Datei -
// gemessen am ersten Lauf, der "Standardmaessiger NVM Express-Controller" als
// "Standardm??iger" ausgab. Ein Bericht, dessen Geraetenamen nicht lesbar
// sind, ist als Entscheidungsgrundlage wertlos.
inline std::string W2U8(const std::wstring& w) {
    if (w.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                      NULL, 0, NULL, NULL);
    if (n <= 0) return std::string();
    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &out[0], n, NULL, NULL);
    return out;
}

// char und nicht wchar_t: der Bericht wird mit fprintf geschrieben, und ein
// wchar_t* an ein %s ist genau die Sorte Fehler, die der Compiler zwar
// anmerkt, die aber Muell in die Datei schreibt statt den Bau anzuhalten.
inline const char* PowerStateName(int d) {
    switch (d) {
        case 1:  return "D0";
        case 2:  return "D1";
        case 3:  return "D2";
        case 4:  return "D3";
        default: return "? ";
    }
}

// Schreibt den Bericht. Getrennte Spalten, keine Aufrundung, UNBEKANNT bleibt
// UNBEKANNT.
inline void WriteReport(FILE* fp, const SystemInfo& sys,
                        const std::vector<DeviceEntry>& devs,
                        unsigned uptimeSeconds) {
    fprintf(fp, "OneClickRGB Energiemanager - Inventar (Phase 4, nur lesend)\n\n");

    fprintf(fp, "=== SYSTEMLAGE ===\n");
    if (!sys.valid) {
        fprintf(fp, "UNBEKANNT: CallNtPowerInformation lieferte nichts.\n"
                    "Ohne diese Quelle ist die Achse blind, nicht leer.\n\n");
    } else {
        fprintf(fp, "  Standby S3            : %s\n", sys.s3 ? "ja" : "nein");
        fprintf(fp, "  Ruhezustand S4        : %s (Datei eingerichtet: %s)\n",
                sys.s4 ? "ja" : "nein", sys.hiberFilePresent ? "ja" : "nein");
        fprintf(fp, "  Modern Standby (AoAc) : %s\n", sys.aoac ? "ja" : "nein");
        fprintf(fp, "  Akku vorhanden        : %s\n\n", sys.batteryPresent ? "ja" : "nein");
        fprintf(fp, "  Gegenprobe: 'powercfg /a' muss dasselbe sagen. Weicht es ab,\n"
                    "  ist diese Sonde falsch, nicht powercfg.\n\n");
    }

    int inD0 = 0, blocked = 0, caution = 0, free_ = 0, freeInD0 = 0;
    for (size_t i = 0; i < devs.size(); ++i) {
        if (devs[i].powerState == 1) inD0++;
        if (devs[i].safety == SAFE_FREE) {
            free_++;
            if (devs[i].powerState == 1) freeInD0++;
        } else if (devs[i].safety == SAFE_CAUTION) {
            caution++;
        } else {
            blocked++;
        }
    }

    fprintf(fp, "=== UEBERSICHT ===\n");
    fprintf(fp, "  Praesente Geraete              : %u\n", (unsigned)devs.size());
    fprintf(fp, "  davon in D0 (voll bestromt)    : %d\n", inD0);
    fprintf(fp, "  Ampel gesperrt/achtung/frei    : %d / %d / %d\n", blocked, caution, free_);
    fprintf(fp, "  davon 'frei' UND in D0         : %d\n", freeInD0);
    fprintf(fp, "  Bezugsfenster der Lastzaehler  : %u s seit Systemstart\n", uptimeSeconds);
    fprintf(fp, "  (Ohne Fenster heisst '0 Bytes' gar nichts.)\n\n");

    // Diese Zeile ist der ehrliche Teil des Berichts. Die gesuchte Sorte Geraet
    // - dauerhaft bestromt UND ungenutzt UND gefahrlos abschaltbar - ist die
    // Schnittmenge der drei Bedingungen, und die ist oft leer.
    if (freeInD0 == 0) {
        fprintf(fp, "  BEFUND: Kein einziges Geraet ist gleichzeitig 'frei' UND in D0.\n"
                    "  Die als frei eingestuften Geraete hat Windows bereits selbst\n"
                    "  heruntergeregelt. Sie abzuschalten spart daher voraussichtlich\n"
                    "  NICHTS an Leistungsaufnahme - es entfernt hoechstens ihre Treiber\n"
                    "  aus dem Speicher. Wer hier einen Gewinn verspricht, hat nicht\n"
                    "  gemessen.\n\n");
    } else {
        fprintf(fp, "  BEFUND: %d Geraet(e) sind 'frei' und stehen trotzdem in D0.\n"
                    "  Nur bei diesen ist ueberhaupt ein Gewinn denkbar - wie gross,\n"
                    "  sagt diese Sonde nicht.\n\n", freeInD0);
    }

    fprintf(fp, "=== VORSCHLAEGE (Ampel 'frei') ===\n");
    fprintf(fp, "Diese Liste ist ein Vorschlag, keine Auswahl. Ob ein Geraet\n");
    fprintf(fp, "gebraucht wird, weiss keine Windows-API - nur Sie.\n\n");
    int shown = 0;
    for (size_t i = 0; i < devs.size(); ++i) {
        const DeviceEntry& d = devs[i];
        if (d.safety != SAFE_FREE) continue;
        fprintf(fp, "  [%s] %s\n", PowerStateName(d.powerState),
                W2U8(d.description).c_str());
        fprintf(fp, "        Klasse %s | Dienst %s | Kinder %d | Last %s\n",
                d.cls.empty()     ? "-" : W2U8(d.cls).c_str(),
                d.service.empty() ? "-" : W2U8(d.service).c_str(),
                d.childCount,
                (d.rxBytes < 0) ? "UNBEKANNT (keine Zaehler fuer diese Klasse)"
                                : "0 Bytes");
        fprintf(fp, "        %s\n", W2U8(d.instanceId).c_str());
        shown++;
    }
    if (shown == 0) fprintf(fp, "  (keine)\n");

    fprintf(fp, "\n=== ACHTUNG ===\n");
    shown = 0;
    for (size_t i = 0; i < devs.size(); ++i) {
        const DeviceEntry& d = devs[i];
        if (d.safety != SAFE_CAUTION) continue;
        fprintf(fp, "  [%s] %-52s %s\n", PowerStateName(d.powerState),
                W2U8(d.description).c_str(), W2U8(d.reason).c_str());
        shown++;
    }
    if (shown == 0) fprintf(fp, "  (keine)\n");

    fprintf(fp, "\n=== GESPERRT (Auszug: was Last aufweist) ===\n");
    shown = 0;
    for (size_t i = 0; i < devs.size(); ++i) {
        const DeviceEntry& d = devs[i];
        if (d.safety != SAFE_BLOCKED) continue;
        if (d.rxBytes <= 0 && d.txBytes <= 0 && !d.mediaConnected) continue;
        fprintf(fp, "  %-52s rx %lld / tx %lld Bytes%s\n",
                W2U8(d.description).c_str(), d.rxBytes, d.txBytes,
                d.mediaConnected ? ", verbunden" : "");
        shown++;
    }
    if (shown == 0) fprintf(fp, "  (keine mit Lastzaehlern)\n");

    fprintf(fp, "\n=== GESPERRT: eigene RGB-Geraete ===\n");
    shown = 0;
    for (size_t i = 0; i < devs.size(); ++i) {
        const DeviceEntry& d = devs[i];
        if (!IsOwnRgbDevice(d.instanceId)) continue;
        fprintf(fp, "  %s\n", W2U8(d.description).c_str());
        shown++;
    }
    if (shown == 0) fprintf(fp, "  (keine gefunden)\n");

    fprintf(fp, "\nWas hier NICHT steht: ob ein Geraet je benutzt wurde. Das sagt\n");
    fprintf(fp, "keine Windows-API. Die vier Angaben oben - D-Zustand, Kindgeraete,\n");
    fprintf(fp, "Treiberdienst, Lastzaehler - stehen getrennt und werden nicht\n");
    fprintf(fp, "ineinander aufgerundet.\n");
}

} // namespace powermgr
