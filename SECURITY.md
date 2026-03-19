# Security Policy

## Reporting a Vulnerability

**IMPORTANT:** Do NOT create a public GitHub issue for security vulnerabilities.

### Secure Reporting

Please email security vulnerabilities to: **security@beastware.team**

Include in your report:
- Description of the vulnerability
- Steps to reproduce (if applicable)
- Potential impact assessment
- Suggested remediation (optional)

**Response Timeline:**
- Initial acknowledgment: Within 24 hours
- Investigation: 48-72 hours
- Status update: Every 7 days
- Fix and patch: As soon as possible (typically 2-4 weeks)

## Supported Versions

| Version | Release Date | End of Life | Status |
|---------|-------------|-------------|--------|
| 1.x | 2026-03-19 | 2027-03-19 | ✅ Active |
| 0.x | 2024-01-01 | 2024-06-01 | ❌ EOL |

**Security Updates:** Only the latest version receives security updates.

## Security Best Practices

OneClickRGB implements the following security measures:

### Code Security
- ✅ Regular CodeQL static analysis
- ✅ Input validation on all user inputs
- ✅ Safe C++ practices (no buffer overflows)
- ✅ No SQL injection (no SQL used)
- ✅ No credentials in code
- ✅ No hardcoded secrets

### Dependency Security
- ✅ Weekly dependency scanning (Dependabot)
- ✅ Automatic security updates
- ✅ Minimal dependencies
- ✅ Trusted sources only

### Network Security
- ✅ No unencrypted data transmission (when applicable)
- ✅ Certificate validation
- ✅ No telemetry/tracking
- ✅ Local-first design

### Build Security
- ✅ Signed releases (when applicable)
- ✅ Reproducible builds
- ✅ CI/CD security
- ✅ No 3rd-party build processes

## Known Security Considerations

### 1. Hardware Communication
OneClickRGB directly communicates with USB devices. This requires:
- Administrative/root privileges
- Valid USB HID permissions
- Trusted device firmware

### 2. User Permissions
- Windows: Administrator access recommended
- Linux: udev rules or sudo
- macOS: May require security approval

### 3. Module Loading
Community modules are loaded dynamically. We recommend:
- Only install modules from trusted sources
- Review module code before installation
- Run in sandboxed environment if untrusted

## Security Recommendations for Users

1. **Keep Updated**: Always use the latest version
2. **Verify Downloads**: Check file hashes on releases
3. **Review Source**: Only use official repositories
4. **Report Issues**: Use secure reporting channel above
5. **Review Code**: Open-source allows security audit

## Security Disclosure Policy

Following **Coordinated Vulnerability Disclosure (CVD)**:

1. **Mandatory Disclosure Window:** 90 days after initial report
2. **Grace Period:** Additional 30 days for exceptional cases  
3. **Public Disclosure:** Only after fix is released
4. **Credit:** Researcher credited (unless they request anonymity)

## Transparency Report

We commit to transparency regarding security:

- Monthly security updates in changelog
- Annual security audit (when resources available)
- Public vulnerability database queries
- Community feedback encouraged

---

**Last Updated:** 2026-03-19  
**Next Review:** 2026-06-19
