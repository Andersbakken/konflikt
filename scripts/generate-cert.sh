#!/bin/bash
# Generate self-signed TLS certificate for Konflikt WSS connections
# The certificate is valid for 365 days and includes common local hostnames

set -e

# Default output directory
OUTPUT_DIR="${1:-$HOME/.config/konflikt}"

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

CERT_FILE="$OUTPUT_DIR/konflikt.crt"
KEY_FILE="$OUTPUT_DIR/konflikt.key"

# Get hostname
HOSTNAME=$(hostname)

echo "Generating self-signed TLS certificate for Konflikt..."
echo "Output directory: $OUTPUT_DIR"
echo "Hostname: $HOSTNAME"

# Generate private key and certificate in one step
openssl req -x509 -newkey rsa:4096 \
    -keyout "$KEY_FILE" \
    -out "$CERT_FILE" \
    -sha256 \
    -days 365 \
    -nodes \
    -subj "/CN=$HOSTNAME/O=Konflikt/OU=Local" \
    -addext "subjectAltName=DNS:$HOSTNAME,DNS:localhost,IP:127.0.0.1"

# Set permissions
chmod 600 "$KEY_FILE"
chmod 644 "$CERT_FILE"

echo ""
echo "Certificate generated successfully!"
echo "  Certificate: $CERT_FILE"
echo "  Private Key: $KEY_FILE"
echo ""
echo "To use TLS with Konflikt, add to your config.json:"
echo "  \"useTLS\": true,"
echo "  \"tlsCertFile\": \"$CERT_FILE\","
echo "  \"tlsKeyFile\": \"$KEY_FILE\""
echo ""
echo "Or use command line options:"
echo "  --tls --tls-cert=$CERT_FILE --tls-key=$KEY_FILE"
echo ""
echo "Note: This is a self-signed certificate. Clients will need to trust it"
echo "or use --tls-insecure (if implemented) to skip verification."
