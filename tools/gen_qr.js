#!/usr/bin/env node
// gen_qr.js - Generate a C array literal of a QR code module grid for the
// Oric screen, for inclusion in src/info.c.
//
// Adapted from locifilemanager-v2's tools/gen_qr.js (same vendored library,
// same alphanumeric-mode bitstream construction) -- only TEXT and the output
// array name/type changed for this project's GitHub URL.
//
// Uses the QR matrix routines (RS error correction, masking, position/timing
// patterns) from the kazuhikoarase/qrcode-generator library, vendored locally
// at /usr/share/nodejs/qrcode-terminal/vendor/QRCode (MIT licensed).
//
// That library only implements 8-bit byte mode (QR8bitByte), which would not
// fit "GITHUB.COM/XAHMOL/ORICSCREENEDITORLOCI" (39 chars) into a 25x25
// (version 2) symbol. This script instead builds the bit stream for QR
// *alphanumeric* mode by hand (mode indicator 0010, 9-bit count for versions
// 1-9, 11 bits per character pair / 6 bits for a trailing single char - QR
// spec table), then hands the resulting data codewords to the vendored
// library's QRCode.createBytes()/makeImpl() for RS encoding, masking and
// matrix layout.
//
// One-time generation step, not a build dependency: run `node gen_qr.js`
// and paste the resulting array literal into src/info.c. No Node.js
// requirement is added to the Makefile/build process.
//
// Output: 625 bytes (25x25), one per module, CH_INVSPACE (0xA0) for dark
// modules and CH_SPACE (0x20) for light modules.

const QRCode = require('/usr/share/nodejs/qrcode-terminal/vendor/QRCode/index.js');
const QRBitBuffer = require('/usr/share/nodejs/qrcode-terminal/vendor/QRCode/QRBitBuffer.js');
const QRRSBlock = require('/usr/share/nodejs/qrcode-terminal/vendor/QRCode/QRRSBlock.js');
const QRErrorCorrectLevel = require('/usr/share/nodejs/qrcode-terminal/vendor/QRCode/QRErrorCorrectLevel.js');

const TEXT = "GITHUB.COM/XAHMOL/ORICSCREENEDITORLOCI";
const ALPHANUM = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";
const CH_SPACE = 0x20;
const CH_INVSPACE = 0xA0;

function alnumVal(c) {
    const idx = ALPHANUM.indexOf(c);
    if (idx < 0) throw new Error("char not in QR alphanumeric set: " + c);
    return idx;
}

const typeNumber = 2; // 25x25 modules
const errorCorrectLevel = QRErrorCorrectLevel.L;

const buffer = new QRBitBuffer();
buffer.put(0b0010, 4);          // mode indicator: alphanumeric
buffer.put(TEXT.length, 9);     // character count indicator (versions 1-9)

for (let i = 0; i < TEXT.length; i += 2) {
    if (i + 1 < TEXT.length) {
        buffer.put(alnumVal(TEXT[i]) * 45 + alnumVal(TEXT[i + 1]), 11);
    } else {
        buffer.put(alnumVal(TEXT[i]), 6);
    }
}

const rsBlocks = QRRSBlock.getRSBlocks(typeNumber, errorCorrectLevel);
let totalDataCount = 0;
for (const b of rsBlocks) totalDataCount += b.dataCount;

if (buffer.getLengthInBits() > totalDataCount * 8) {
    throw new Error("data too long for QR version " + typeNumber);
}

if (buffer.getLengthInBits() + 4 <= totalDataCount * 8) {
    buffer.put(0, 4); // terminator
}

while (buffer.getLengthInBits() % 8 !== 0) {
    buffer.putBit(false);
}

while (true) {
    if (buffer.getLengthInBits() >= totalDataCount * 8) break;
    buffer.put(QRCode.PAD0, 8);
    if (buffer.getLengthInBits() >= totalDataCount * 8) break;
    buffer.put(QRCode.PAD1, 8);
}

const dataCache = QRCode.createBytes(buffer, rsBlocks);

const qr = new QRCode(typeNumber, errorCorrectLevel);
qr.dataCache = dataCache;
const maskPattern = qr.getBestMaskPattern();
qr.makeImpl(false, maskPattern);

const moduleCount = qr.getModuleCount();
if (moduleCount !== 25) {
    throw new Error("unexpected module count: " + moduleCount);
}

const out = [];
for (let row = 0; row < moduleCount; row++) {
    for (let col = 0; col < moduleCount; col++) {
        out.push(qr.isDark(row, col) ? CH_INVSPACE : CH_SPACE);
    }
}

// Emit as a C array literal, 25 values per line (one QR row per line)
const lines = [];
for (let row = 0; row < moduleCount; row++) {
    const rowBytes = out.slice(row * moduleCount, (row + 1) * moduleCount);
    lines.push("    " + rowBytes.map(b => "0x" + b.toString(16)).join(", ") + ",");
}

console.log(`// Encodes "${TEXT}" (QR version ${typeNumber}, ${moduleCount}x${moduleCount}, EC level L)`);
console.log("static const uint8_t qr_github[" + out.length + "] = {");
console.log(lines.join("\n"));
console.log("};");
