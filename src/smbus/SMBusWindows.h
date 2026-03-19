/*---------------------------------------------------------*\
| SMBusWindows.h                                            |
|                                                           |
| Windows SMBus implementations using WinRing0/inpoutx64    |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#pragma once

#include "SMBusInterface.h"
#include <windows.h>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| WinRing0 SMBus Implementation                             |
| Uses the WinRing0 library for direct hardware access      |
\*---------------------------------------------------------*/
class WinRing0SMBus : public SMBusInterface {
public:
    WinRing0SMBus();
    ~WinRing0SMBus() override;

    // SMBusInterface implementation
    bool Initialize() override;
    void Shutdown() override;
    bool IsInitialized() const override { return m_initialized; }

    SMBusControllerInfo GetControllerInfo() const override { return m_controller_info; }

    bool QuickCommand(uint8_t addr, bool read) override;
    bool SendByte(uint8_t addr, uint8_t data) override;
    uint8_t ReceiveByte(uint8_t addr) override;
    bool WriteByte(uint8_t addr, uint8_t reg, uint8_t data) override;
    uint8_t ReadByte(uint8_t addr, uint8_t reg) override;
    bool WriteWord(uint8_t addr, uint8_t reg, uint16_t data) override;
    uint16_t ReadWord(uint8_t addr, uint8_t reg) override;
    bool WriteBlock(uint8_t addr, uint8_t reg,
                    const uint8_t* data, uint8_t length) override;
    int ReadBlock(uint8_t addr, uint8_t reg,
                  uint8_t* buffer, uint8_t max_length) override;

protected:
    bool WaitForNotBusy(int timeout_us = 10000) override;
    bool WaitForComplete(int timeout_us = 10000) override;

private:
    HMODULE m_dll_handle = nullptr;

    // WinRing0 function pointers
    typedef BOOL (WINAPI *InitializeOlsType)(void);
    typedef VOID (WINAPI *DeinitializeOlsType)(void);
    typedef DWORD (WINAPI *GetDllStatusType)(void);
    typedef BYTE (WINAPI *ReadIoPortByteType)(WORD port);
    typedef VOID (WINAPI *WriteIoPortByteType)(WORD port, BYTE value);
    typedef WORD (WINAPI *ReadIoPortWordType)(WORD port);
    typedef VOID (WINAPI *WriteIoPortWordType)(WORD port, WORD value);
    typedef BOOL (WINAPI *ReadPciConfigDwordExType)(DWORD pciAddress, DWORD regAddress, PDWORD value);
    typedef DWORD (WINAPI *FindPciDeviceByIdType)(WORD vendorId, WORD deviceId, BYTE index);

    InitializeOlsType       m_InitializeOls = nullptr;
    DeinitializeOlsType     m_DeinitializeOls = nullptr;
    GetDllStatusType        m_GetDllStatus = nullptr;
    ReadIoPortByteType      m_ReadIoPortByte = nullptr;
    WriteIoPortByteType     m_WriteIoPortByte = nullptr;
    ReadIoPortWordType      m_ReadIoPortWord = nullptr;
    WriteIoPortWordType     m_WriteIoPortWord = nullptr;
    ReadPciConfigDwordExType m_ReadPciConfigDwordEx = nullptr;
    FindPciDeviceByIdType   m_FindPciDeviceById = nullptr;

    // Helper methods
    bool LoadWinRing0();
    void UnloadWinRing0();
    bool DetectSMBusController();
    bool DetectIntelSMBus();
    bool DetectAMDSMBus();

    void ClearStatus();
    uint8_t GetStatus();

    // Port I/O wrappers
    uint8_t InPortByte(uint16_t port);
    void OutPortByte(uint16_t port, uint8_t value);
    uint16_t InPortWord(uint16_t port);
    void OutPortWord(uint16_t port, uint16_t value);

    // PCI config access
    uint32_t ReadPciConfig(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg);
};

/*---------------------------------------------------------*\
| inpoutx64 SMBus Implementation                            |
| Alternative library for direct hardware access            |
\*---------------------------------------------------------*/
class InpOutSMBus : public SMBusInterface {
public:
    InpOutSMBus();
    ~InpOutSMBus() override;

    // SMBusInterface implementation
    bool Initialize() override;
    void Shutdown() override;
    bool IsInitialized() const override { return m_initialized; }

    SMBusControllerInfo GetControllerInfo() const override { return m_controller_info; }

    bool QuickCommand(uint8_t addr, bool read) override;
    bool SendByte(uint8_t addr, uint8_t data) override;
    uint8_t ReceiveByte(uint8_t addr) override;
    bool WriteByte(uint8_t addr, uint8_t reg, uint8_t data) override;
    uint8_t ReadByte(uint8_t addr, uint8_t reg) override;
    bool WriteWord(uint8_t addr, uint8_t reg, uint16_t data) override;
    uint16_t ReadWord(uint8_t addr, uint8_t reg) override;
    bool WriteBlock(uint8_t addr, uint8_t reg,
                    const uint8_t* data, uint8_t length) override;
    int ReadBlock(uint8_t addr, uint8_t reg,
                  uint8_t* buffer, uint8_t max_length) override;

protected:
    bool WaitForNotBusy(int timeout_us = 10000) override;
    bool WaitForComplete(int timeout_us = 10000) override;

private:
    HMODULE m_dll_handle = nullptr;

    // inpoutx64 function pointers
    typedef void (WINAPI *Out32Type)(short port, short data);
    typedef short (WINAPI *Inp32Type)(short port);
    typedef BOOL (WINAPI *IsInpOutDriverOpenType)(void);
    typedef BOOL (WINAPI *IsDriverOpen64Type)(void);

    Out32Type               m_Out32 = nullptr;
    Inp32Type               m_Inp32 = nullptr;
    IsInpOutDriverOpenType  m_IsDriverOpen = nullptr;

    // Helper methods
    bool LoadInpOut();
    void UnloadInpOut();
    bool DetectSMBusController();

    void ClearStatus();
    uint8_t GetStatus();

    // Port I/O wrappers
    uint8_t InPortByte(uint16_t port);
    void OutPortByte(uint16_t port, uint8_t value);
};

} // namespace OneClickRGB
