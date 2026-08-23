package utils

import (
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"encoding/base64"
	"errors"
	"io"
	"os"
)

// GetEncryptionKey retrieves the encryption key from env or uses a default (for development only)
func GetEncryptionKey() []byte {
	key := os.Getenv("ENCRYPTION_KEY")
	if len(key) == 0 {
		key = "default_secret_key_32_bytes_long_" // 32 bytes for AES-256
	}
	// Pad or truncate to 32 bytes
	b := []byte(key)
	if len(b) > 32 {
		b = b[:32]
	} else if len(b) < 32 {
		padded := make([]byte, 32)
		copy(padded, b)
		b = padded
	}
	return b
}

// Encrypt encrypts plain text using AES-GCM
func Encrypt(plaintext string) (string, error) {
	if plaintext == "" {
		return "", nil
	}
	block, err := aes.NewCipher(GetEncryptionKey())
	if err != nil {
		return "", err
	}

	aesGCM, err := cipher.NewGCM(block)
	if err != nil {
		return "", err
	}

	nonce := make([]byte, aesGCM.NonceSize())
	if _, err = io.ReadFull(rand.Reader, nonce); err != nil {
		return "", err
	}

	ciphertext := aesGCM.Seal(nonce, nonce, []byte(plaintext), nil)
	return base64.StdEncoding.EncodeToString(ciphertext), nil
}

// Decrypt decrypts cipher text using AES-GCM
func Decrypt(encryptedString string) (string, error) {
	if encryptedString == "" {
		return "", nil
	}
	enc, err := base64.StdEncoding.DecodeString(encryptedString)
	if err != nil {
		return "", err
	}

	block, err := aes.NewCipher(GetEncryptionKey())
	if err != nil {
		return "", err
	}

	aesGCM, err := cipher.NewGCM(block)
	if err != nil {
		return "", err
	}

	nonceSize := aesGCM.NonceSize()
	if len(enc) < nonceSize {
		return "", errors.New("ciphertext too short")
	}

	nonce, ciphertext := enc[:nonceSize], enc[nonceSize:]
	plaintext, err := aesGCM.Open(nil, nonce, ciphertext, nil)
	if err != nil {
		return "", err
	}

	return string(plaintext), nil
}
