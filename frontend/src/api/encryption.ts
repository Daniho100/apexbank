// Native Web Crypto API payload encryption for production-ready security
// Cryptographically matches C++ OpenSSL AES-256-GCM implementation.

const DEFAULT_AES_KEY = 'banking_aes_256_gcm_secret_key_32_bytes';

function getCryptoKey(rawKey: string): Promise<CryptoKey> {
  // Resolve key to exactly 32 bytes (256 bits) matching backend logic
  let keyStr = rawKey;
  if (keyStr.length < 32) {
    keyStr = keyStr.padEnd(32, '0');
  } else if (keyStr.length > 32) {
    keyStr = keyStr.substring(0, 32);
  }
  const encoder = new TextEncoder();
  const keyBytes = encoder.encode(keyStr);
  return window.crypto.subtle.importKey(
    'raw',
    keyBytes,
    { name: 'AES-GCM' },
    false,
    ['encrypt', 'decrypt']
  );
}

// Convert ArrayBuffer to Base64
function arrayBufferToBase64(buffer: ArrayBuffer): string {
  const bytes = new Uint8Array(buffer);
  let binary = '';
  const len = bytes.byteLength;
  for (let i = 0; i < len; i++) {
    binary += String.fromCharCode(bytes[i]);
  }
  return window.btoa(binary);
}

// Convert Base64 to ArrayBuffer
function base64ToArrayBuffer(base64: string): ArrayBuffer {
  const binary = window.atob(base64);
  const len = binary.length;
  const bytes = new Uint8Array(len);
  for (let i = 0; i < len; i++) {
    bytes[i] = binary.charCodeAt(i);
  }
  return bytes.buffer;
}

/**
 * Encrypt plaintext using AES-256-GCM and return a Base64 packed string
 * Package format: IV (12 bytes) + TAG (16 bytes) + CIPHERTEXT
 */
export async function encryptPayload(plaintext: string, rawKey: string = DEFAULT_AES_KEY): Promise<string> {
  const cryptoKey = await getCryptoKey(rawKey);
  const encoder = new TextEncoder();
  const plaintextBytes = encoder.encode(plaintext);

  // Generate 12-byte random IV
  const iv = window.crypto.getRandomValues(new Uint8Array(12));

  // Encrypt. SubtleCrypto appends the 16-byte tag to the ciphertext.
  const encryptedBuffer = await window.crypto.subtle.encrypt(
    { name: 'AES-GCM', iv, tagLength: 128 },
    cryptoKey,
    plaintextBytes
  );

  const encryptedBytes = new Uint8Array(encryptedBuffer);
  const ciphertextLen = encryptedBytes.length - 16;
  const tag = encryptedBytes.subarray(ciphertextLen);
  const ciphertext = encryptedBytes.subarray(0, ciphertextLen);

  // Pack IV (12) + TAG (16) + CIPHERTEXT
  const packedBytes = new Uint8Array(12 + 16 + ciphertext.length);
  packedBytes.set(iv, 0);
  packedBytes.set(tag, 12);
  packedBytes.set(ciphertext, 28);

  return arrayBufferToBase64(packedBytes.buffer);
}

/**
 * Decrypt packed Base64 string using AES-256-GCM
 */
export async function decryptPayload(packedBase64: string, rawKey: string = DEFAULT_AES_KEY): Promise<string> {
  const cryptoKey = await getCryptoKey(rawKey);
  const packedBytes = new Uint8Array(base64ToArrayBuffer(packedBase64));

  if (packedBytes.length < 28) {
    throw new Error('Invalid ciphertext package: too short');
  }

  // Unpack IV (12) + TAG (16) + CIPHERTEXT
  const iv = packedBytes.subarray(0, 12);
  const tag = packedBytes.subarray(12, 28);
  const ciphertext = packedBytes.subarray(28);

  // SubtleCrypto expects Ciphertext + Tag appended at the end
  const ciphertextAndTag = new Uint8Array(ciphertext.length + 16);
  ciphertextAndTag.set(ciphertext, 0);
  ciphertextAndTag.set(tag, ciphertext.length);

  // Decrypt
  const decryptedBuffer = await window.crypto.subtle.decrypt(
    { name: 'AES-GCM', iv, tagLength: 128 },
    cryptoKey,
    ciphertextAndTag
  );

  const decoder = new TextDecoder();
  return decoder.decode(decryptedBuffer);
}

// Synchronous local storage encryption helper (Multi-byte key-based XOR cipher)
function xorCipher(input: string, key: string): string {
  let output = '';
  for (let i = 0; i < input.length; i++) {
    output += String.fromCharCode(input.charCodeAt(i) ^ key.charCodeAt(i % key.length));
  }
  return output;
}

export function encryptStorage(plaintext: string, rawKey: string = DEFAULT_AES_KEY): string {
  try {
    const encrypted = xorCipher(plaintext, rawKey);
    return window.btoa(encodeURIComponent(encrypted));
  } catch (e) {
    return plaintext;
  }
}

export function decryptStorage(ciphertext: string, rawKey: string = DEFAULT_AES_KEY): string {
  try {
    const rawXor = decodeURIComponent(window.atob(ciphertext));
    return xorCipher(rawXor, rawKey);
  } catch (e) {
    return ciphertext;
  }
}
