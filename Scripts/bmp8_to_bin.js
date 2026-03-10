#!/usr/bin/env node

const fs = require("fs");
const path = require("path");

function readUInt16LE(buffer, offset) {
  return buffer.readUInt16LE(offset);
}

function readUInt32LE(buffer, offset) {
  return buffer.readUInt32LE(offset);
}

function readInt32LE(buffer, offset) {
  return buffer.readInt32LE(offset);
}

function printUsage() {
  const scriptName = path.basename(process.argv[1] || "bmp8_to_bin.js");
  console.log(`Usage: node ${scriptName} <input.bmp> [output.bin]\n`);
  console.log("Converts an uncompressed 8-bit BMP to a raw .bin file of palette indices.");
  console.log("Each output byte is one pixel index, written in row-major top-to-bottom order.");
}

function toBinPath(inputPath) {
  const parsed = path.parse(inputPath);
  return path.join(parsed.dir, `${parsed.name}.bin`);
}

function main() {
  const [, , inputPathArg, outputPathArg] = process.argv;

  if (!inputPathArg || inputPathArg === "-h" || inputPathArg === "--help") {
    printUsage();
    process.exit(inputPathArg ? 0 : 1);
  }

  const inputPath = path.resolve(process.cwd(), inputPathArg);
  const outputPath = path.resolve(process.cwd(), outputPathArg || toBinPath(inputPathArg));

  let bmp;
  try {
    bmp = fs.readFileSync(inputPath);
  } catch (err) {
    console.error(`Failed to read input BMP: ${inputPath}`);
    console.error(err.message);
    process.exit(1);
  }

  if (bmp.length < 54) {
    console.error("Input file is too small to be a valid BMP.");
    process.exit(1);
  }

  if (bmp.toString("ascii", 0, 2) !== "BM") {
    console.error("Unsupported file format: expected BMP signature 'BM'.");
    process.exit(1);
  }

  const pixelDataOffset = readUInt32LE(bmp, 10);
  const dibHeaderSize = readUInt32LE(bmp, 14);

  if (dibHeaderSize < 40) {
    console.error(`Unsupported DIB header size: ${dibHeaderSize}. Expected at least 40 bytes.`);
    process.exit(1);
  }

  const width = readInt32LE(bmp, 18);
  const heightSigned = readInt32LE(bmp, 22);
  const planes = readUInt16LE(bmp, 26);
  const bitCount = readUInt16LE(bmp, 28);
  const compression = readUInt32LE(bmp, 30);

  if (width <= 0 || heightSigned === 0) {
    console.error(`Unsupported dimensions: width=${width}, height=${heightSigned}`);
    process.exit(1);
  }

  if (planes !== 1) {
    console.error(`Unsupported BMP planes value: ${planes}. Expected 1.`);
    process.exit(1);
  }

  if (bitCount !== 8) {
    console.error(`Unsupported BMP bit depth: ${bitCount}. Expected 8-bit indexed BMP.`);
    process.exit(1);
  }

  if (compression !== 0) {
    console.error(`Unsupported BMP compression: ${compression}. Expected BI_RGB (0).`);
    process.exit(1);
  }

  const height = Math.abs(heightSigned);
  const isTopDown = heightSigned < 0;

  const rowStride = ((width + 3) >> 2) << 2;
  const expectedPixelBytes = rowStride * height;

  if (pixelDataOffset + expectedPixelBytes > bmp.length) {
    console.error("BMP pixel data is truncated or header offsets are invalid.");
    process.exit(1);
  }

  const out = Buffer.alloc(width * height);

  // Convert BMP row storage (possibly bottom-up, with 4-byte row padding) to packed rows.
  for (let y = 0; y < height; y++) {
    const srcY = isTopDown ? y : height - 1 - y;
    const srcRowOffset = pixelDataOffset + srcY * rowStride;
    const dstRowOffset = y * width;

    bmp.copy(out, dstRowOffset, srcRowOffset, srcRowOffset + width);
  }

  try {
    fs.writeFileSync(outputPath, out);
  } catch (err) {
    console.error(`Failed to write output .bin file: ${outputPath}`);
    console.error(err.message);
    process.exit(1);
  }

  console.log(`Wrote ${out.length} bytes (${width}x${height}) to ${outputPath}`);
}

main();
