#!/usr/bin/env bash
set -e

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=============================="
echo "🔨 Build x64 (host)"
echo "=============================="

# Limpiar y reconstruir EXACTAMENTE como lo haces manualmente
echo "🧹 Limpiando build-x64..."
rm -rf "$ROOT_DIR/build-x64"

echo "📁 Creando directorio build-x64..."
mkdir -p "$ROOT_DIR/build-x64"
cd "$ROOT_DIR/build-x64"

echo "⚙️  Configurando CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

echo "🔨 Compilando..."
cmake --build . --verbose

# Verificar el ejecutable
if [ -f "./auto_sync_server" ]; then
    echo "✅ Ejecutable x64 generado correctamente"
    file ./auto_sync_server
    echo "📏 Tamaño: $(stat -c%s ./auto_sync_server) bytes"
else
    echo "❌ Error: no se generó el ejecutable x64"
    exit 1
fi

echo
echo "=============================="
echo "🔨 Build ARM64 (cross)"
echo "=============================="

# Limpiar y reconstruir para ARM64 de manera similar
echo "🧹 Limpiando build-arm64..."
rm -rf "$ROOT_DIR/build-arm64"

echo "📁 Creando directorio build-arm64..."
mkdir -p "$ROOT_DIR/build-arm64"
cd "$ROOT_DIR/build-arm64"

echo "⚙️  Configurando CMake para ARM64..."
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT_DIR/toolchain-arm64.cmake" \
  -DCMAKE_BUILD_TYPE=Release

echo "🔨 Compilando para ARM64..."
cmake --build . --verbose

# Verificar el ejecutable ARM64
if [ -f "./auto_sync_server" ]; then
    echo "✅ Ejecutable ARM64 generado correctamente"
    file ./auto_sync_server
    echo "📏 Tamaño: $(stat -c%s ./auto_sync_server) bytes"
else
    echo "❌ Error: no se generó el ejecutable ARM64"
    exit 1
fi

echo
echo "=============================="
echo "✅ Builds completados"
echo "=============================="
echo "📦 x64:   $ROOT_DIR/build-x64/auto_sync_server"
echo "📦 ARM64: $ROOT_DIR/build-arm64/auto_sync_server"
echo "=============================="