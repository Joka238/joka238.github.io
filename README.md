# telepaint viewer – E2E (GitHub Pages)

Statischer Viewer, der **verschlüsselte** oder **unverschlüsselte** Stroke-JSONs rendert.
- Verschlüsselte Envelopes: AES-256-GCM, Key aus Passwort via PBKDF2 (SHA-256, 200k Iterationen)
- Klartext-JSON wird weiterhin verstanden, damit der Rollout sanft ist.

## Deploy
1. Inhalt dieses Ordners in dein `USERNAME.github.io` Repository kopieren.
2. Commit + Push. Aufruf: https://USERNAME.github.io/

## JSON-Formate
### Klartext (Kompatibilität)
```json
{
  "session": "abc123",
  "user": "aljoscha",
  "ts": 1699988776,
  "strokes": [ { "id":"s1","color":"#000","width":3,"points":[[0.1,0.2,1699],[0.12,0.21,1700]] } ]
}
```

### Verschlüsselte Envelope (empfohlen)
```json
{
  "v": 1,
  "alg": "AES-256-GCM",
  "salt": "<base64>",
  "iv": "<base64>",
  "ct": "<base64>"   // Ciphertext inkl. Auth-Tag (WebCrypto/openssl kompatibel)
}
```
- `salt` = 16 Byte, zufällig
- `iv`   = 12 Byte, zufällig
- `ct`   = Ciphertext (inkl. 16-Byte GCM-Tag am Ende)

## Passwort-Eingabe
- Der Viewer fragt nach einem Passwort, wenn eine verschlüsselte Datei erkannt wird.
- Das Passwort bleibt nur im RAM, kein Storage.

## C++-Client (Kurzinfo)
- Vor dem Upload: JSON serialisieren → per PBKDF2 Key ableiten → AES-GCM verschlüsseln → Envelope als JSON hochladen.
- Für PBKDF2/AES auf C++ wahlweise OpenSSL/LibreSSL verwenden.
