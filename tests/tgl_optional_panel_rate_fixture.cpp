
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

constexpr uint32_t kIOReturnBusy = 0xe00002d5;
constexpr uint32_t kIOReturnIOError = 0xe00002ca;
struct XE {
    using Fn = uint32_t (*)(void *, uint32_t, void *, uint32_t);
    static uint32_t wrapTGLReadAUX(void *, uint32_t, void *, uint32_t);
    static uint32_t wrapTGLWriteAUX(void *, uint32_t, void *, uint32_t);
    Fn orgTGLReadAUX, orgTGLWriteAUX;
    bool tglBootstrapAUXReady = false;
    uint32_t tglOptionalPanelAUXBusy = 0;
    bool tglOptionalPanelPort(void *p) const { return p == this; }
};
XE *callbackXE;
template<class F> F FunctionCast(F, F original) { return original; }
template<class Operation> uint32_t tglInvokeBootstrapAUX(bool, unsigned, void *, Operation fn) { return fn(); }
void tglDPCDRecord(void *, uint32_t, void *, uint32_t, bool, bool, uint32_t, void *) {}
void IODelay(unsigned n) { assert(n == 100); }
struct IORegistryEntry {
    static IORegistryEntry *getRegistryRoot() { static IORegistryEntry entry; return &entry; }
    void setProperty(const char *, uint32_t index, int) { assert(index == 3 || index == 7); }
};

uint8_t registers[0x200] {0x14, 0, 0xc2, 0x41};
uint32_t failingAddress = 0xffffffff;
bool loseIndex;
std::vector<uint32_t> writes;
uint32_t nativeRead(void *, uint32_t address, void *data, uint32_t length) {
    memcpy(data, registers + address, length);
    return 0;
}
uint32_t nativeWrite(void *, uint32_t address, void *data, uint32_t length) {
    writes.push_back(address);
    if (address == failingAddress) return 15;
    memcpy(registers + address, data, length);
    if (address == 0x100 && loseIndex) registers[0x115] &= 0xf8;
    return 0;
}

#include "tgl_optional_panel_rate_wrappers.inc"

int main() {
    const uint8_t rates[] {0xa4, 0x1f, 0x30, 0x2a, 0x76, 0x2f, 0xbc, 0x34,
                           0x48, 0x3f, 0xd4, 0x49, 0x60, 0x54, 0x78, 0x69};
    memcpy(registers + 0x10, rates, sizeof rates);
    XE xe {nativeRead, nativeWrite};
    callbackXE = &xe;
    uint8_t caps[4] {};
    assert(!XE::wrapTGLReadAUX(&xe, 0, caps, 4) && caps[1] == 0x14 && registers[1] == 0);
    assert(!XE::wrapTGLReadAUX(nullptr, 0, caps, 4) && caps[1] == 0);
    assert(!XE::wrapTGLReadAUX(&xe, 1, caps, 1) && caps[0] == 0x14);

    registers[0x115] = 0xa0;
    uint8_t request[] {0x14, 0x82};
    assert(!XE::wrapTGLWriteAUX(&xe, 0x100, request, 2));
    assert((writes == std::vector<uint32_t>{0x115, 0x100}));
    assert(registers[0x100] == 0 && registers[0x101] == 0x82 && registers[0x115] == 0xa7);
    assert(request[0] == 0x14);
    assert(!XE::wrapTGLReadAUX(&xe, 0x100, caps, 2) && caps[0] == 0x14 && caps[1] == 0x82);

    request[0] = 0x0a;
    assert(!XE::wrapTGLWriteAUX(&xe, 0x100, request, 2) && registers[0x115] == 0xa3);
    assert(!XE::wrapTGLReadAUX(&xe, 0x100, caps, 1) && caps[0] == 0x0a);
    for (void *port : {static_cast<void *>(nullptr), static_cast<void *>(&xe)}) {
        if (port) registers[0x10] ^= 1;
        writes.clear();
        assert(!XE::wrapTGLWriteAUX(port, 0x100, request, 2));
        assert((writes == std::vector<uint32_t>{0x100}) && registers[0x100] == 0x0a);
        if (port) registers[0x10] ^= 1;
    }

    writes.clear();
    xe.tglOptionalPanelAUXBusy = 1;
    assert(XE::wrapTGLWriteAUX(&xe, 0x100, request, 2) == kIOReturnBusy && writes.empty());
    xe.tglOptionalPanelAUXBusy = 0;
    failingAddress = 0x115;
    assert(XE::wrapTGLWriteAUX(&xe, 0x100, request, 2) == 15);
    assert((writes == std::vector<uint32_t>{0x115}));
    failingAddress = 0xffffffff;
    loseIndex = true;
    assert(XE::wrapTGLWriteAUX(&xe, 0x100, request, 2) == kIOReturnIOError);
    assert(xe.tglOptionalPanelAUXBusy == 0);
}
