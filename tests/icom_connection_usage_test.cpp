#include "core/backends/icom/IcomProtocol.h"

#include <algorithm>
#include <cstdio>

using namespace AetherSDR::icom;
namespace {
int failures = 0;
void check(bool ok, const char* label)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) { ++failures; }
}
using Decision = AuditConnectionDecision;
AuditConnectionContext context()
{
    return {0x12345678, 0x87654321, {1,2,3,4,5,6}, {7,8,9},
            true, true, true, true, false, "HF5A-0123456789", 0xc0a80102};
}
std::vector<std::uint8_t> packet(const AuditConnectionContext& c)
{
    std::vector<std::uint8_t> p(kLenConnInfo, 0);
    writeHeader(p, {kLenConnInfo, 0, 3, c.remoteSid, c.localSid});
    p[0x13] = 0x80; p[0x14] = 3; p[0x60] = 1;
    std::copy(c.authId.begin(), c.authId.end(), p.begin() + 0x1a);
    std::copy(c.radioId.begin(), c.radioId.end(), p.begin() + 0x20);
    std::copy(c.ownerName.begin(), c.ownerName.end(), p.begin() + 0x64);
    for (int i = 0; i < 4; ++i) {
        p[0x84 + i] = static_cast<std::uint8_t>(c.ownerIpv4 >> (24 - 8 * i));
    }
    return p;
}
}

int main()
{
    const auto c = context();
    const auto own = packet(c);
    const auto decision = [&](const auto& p) { return classifyAuditConnectionUsage(p, c); };
    check(parseConnectionUsage(own).kind == ConnectionUsageKind::Busy
          && parseStreamGrant(own).granted && decision(own) == Decision::Grant,
          "overlapping wire parsers resolve to own grant only with complete context");
    check(parseConnectionUsage(own).ownerIpv4 == 0xc0a80102
          && parseConnectionUsage(own).ownerName == c.ownerName,
          "owner label and IPv4 are decoded at 0x64 and network-order 0x84");
    auto p = own; p[0x64] = 'x';
    check(decision(p) == Decision::Busy, "another owner on same IP remains Busy");
    p = own; p[0x87] ^= 1;
    check(decision(p) == Decision::Busy, "same owner label on another IP remains Busy");
    for (auto offset : {0x08, 0x0c, 0x1a, 0x1f, 0x20, 0x2f}) {
        p = own; p[offset] ^= 1;
        check(decision(p) == Decision::Ignore, "stale SID/AuthId/RadioId cannot grant or report Busy");
        p[0x64] = 'x';
        check(decision(p) == Decision::Ignore, "foreign association Busy is ignored before ownership");
    }
    for (auto offset : {0x00, 0x04, 0x13, 0x14, 0x15, 0x61, 0x63}) {
        p = own; p[offset] ^= 1;
        check(decision(p) == Decision::Ambiguous, "malformed envelope/full-width busy fails closed");
    }
    p = own; p.pop_back();
    check(decision(p) == Decision::Ambiguous, "truncated conninfo rejected");
    p = own; p.push_back(0);
    check(decision(p) == Decision::Ambiguous, "oversized conninfo rejected");
    p = own; p[0x64] = 0;
    check(decision(p) == Decision::Ambiguous, "hidden owner is not proof of own grant");
    p = own; std::fill(p.begin() + 0x84, p.begin() + 0x88, 0);
    check(decision(p) == Decision::Ambiguous, "missing owner IP rejected");
    auto changed = c; changed.ownerIpv4 = 0;
    check(classifyAuditConnectionUsage(own, changed) == Decision::Ambiguous, "unknown routed IP rejected");
    changed = c; changed.ownerName.clear();
    check(classifyAuditConnectionUsage(own, changed) == Decision::Ambiguous, "missing login label rejected");
    changed = c; changed.availableBeforeRequest = false;
    check(classifyAuditConnectionUsage(own, changed) == Decision::Ambiguous, "no prior availability means no grant");
    changed = c; changed.streamsRequested = false;
    check(classifyAuditConnectionUsage(own, changed) == Decision::Ambiguous, "unsolicited own-looking grant rejected");
    changed = c; changed.streamGranted = true;
    check(classifyAuditConnectionUsage(own, changed) == Decision::OwnSession, "duplicate grant distinguished");
    changed = c; changed.authenticated = false;
    check(classifyAuditConnectionUsage(own, changed) == Decision::Ignore, "pre-auth notification cannot advance negotiation");
    changed = c; changed.haveRadioId = false;
    check(classifyAuditConnectionUsage(own, changed) == Decision::Ignore, "unselected radio cannot grant");
    p = own; p[0x60] = 0; p[0x64] = 0;
    std::fill(p.begin() + 0x84, p.begin() + 0x88, 0);
    check(decision(p) == Decision::Available, "explicit owner-free notification allows negotiation");
    p[0x84] = 1;
    check(decision(p) == Decision::Ambiguous, "available plus owner IP is contradictory");
    const auto login = buildLogin(1, 2, 3, 4, "user", "pass", c.ownerName);
    check(std::equal(c.ownerName.begin(), c.ownerName.end(), login.begin() + 0x60)
          && login[0x6f] == 0, "login carries exact bounded per-session owner label");
    std::printf("failures=%d\n", failures);
    return failures ? 1 : 0;
}
