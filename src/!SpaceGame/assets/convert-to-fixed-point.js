const fs = require('fs');
const path = require('path');

const inputFile = path.join(__dirname, 'Untitled.obj');
const outputFile = path.join(__dirname, 'ship_obj');

const FIXED_POINT_SCALE = 65536; // 2^16 for 16:16 fixed point

function convertToFixedPoint(value) {
  return Math.floor(parseFloat(value) * FIXED_POINT_SCALE);
}

function processObjFile(inputPath, outputPath) {
  const content = fs.readFileSync(inputPath, 'utf-8');
  const lines = content.split('\n');
  const output = [];

  for (const line of lines) {
    const trimmed = line.trim();
    
    // Handle vertex positions (v)
    if (trimmed.startsWith('v ')) {
      const parts = trimmed.split(/\s+/);
      const x = convertToFixedPoint(parts[1]);
      const y = convertToFixedPoint(parts[2]);
      const z = convertToFixedPoint(parts[3]);
      output.push(`v ${x} ${y} ${z}`);
    }
    // Handle vertex normals (vn)
    else if (trimmed.startsWith('vn ')) {
      const parts = trimmed.split(/\s+/);
      const x = convertToFixedPoint(parts[1]);
      const y = convertToFixedPoint(parts[2]);
      const z = convertToFixedPoint(parts[3]);
      output.push(`vn ${x} ${y} ${z}`);
    }
    // Keep all other lines as-is (faces, groups, etc.)
    else {
      output.push(line);
    }
  }

  fs.writeFileSync(outputPath, output.join('\n'), 'utf-8');
  console.log(`Conversion complete. Output written to ${outputPath}`);
}

processObjFile(inputFile, outputFile);