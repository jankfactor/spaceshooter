const fs = require('fs');
const path = require('path');

const inputFile = process.argv[2];
const outputFile = process.argv[3] || path.join(path.dirname(inputFile), 'ship_obj');

if (!inputFile) {
  console.error('Usage: node convert-to-fixed-point.js <input.obj> [output_file]');
  process.exit(1);
}

const FIXED_POINT_SCALE = 65536; // 2^16 for 16:16 fixed point

function convertToFixedPoint(value) {
  return Math.floor(parseFloat(value) * FIXED_POINT_SCALE);
}

function processObjFile(inputPath, outputPath) {
  const content = fs.readFileSync(inputPath, 'utf-8');
  const lines = content.split('\n');
  
  const vertices = [];
  const normals = [];
  const faces = [];
  let currentMaterial = 0;

  // First pass: collect all data
  for (const line of lines) {
    const trimmed = line.trim();
    
    // Handle vertex positions (v)
    if (trimmed.startsWith('v ') && !trimmed.startsWith('vn ')) {
      const parts = trimmed.split(/\s+/);
      const x = convertToFixedPoint(parts[1]);
      const y = convertToFixedPoint(parts[2]);
      const z = convertToFixedPoint(parts[3]);
      vertices.push(`${x} ${y} ${z}`);
    }
    // Handle vertex normals (vn)
    else if (trimmed.startsWith('vn ')) {
      const parts = trimmed.split(/\s+/);
      const x = convertToFixedPoint(parts[1]);
      const y = convertToFixedPoint(parts[2]);
      const z = convertToFixedPoint(parts[3]);
      normals.push(`${x} ${y} ${z}`);
    }
    // Track current material index (usemtl <number>)
    else if (trimmed.startsWith('usemtl ')) {
      const parts = trimmed.split(/\s+/);
      const parsedMaterial = parseInt(parts[1], 10);
      if (!Number.isNaN(parsedMaterial)) {
        currentMaterial = parsedMaterial;
      }
    }
    // Handle faces (f)
    else if (trimmed.startsWith('f ')) {
      // Parse faces in format: f v1//n1 v2//n2 v3//n3
      const parts = trimmed.substring(2).trim().split(/\s+/);
      if (parts.length >= 3) {
        const indices = parts.map(p => {
          const [v, , n] = p.split('/');
          return { v: parseInt(v), n: parseInt(n) };
        });
        // Store as: mat v1 n1 v2 n2 v3 n3
        faces.push(`${currentMaterial} ${indices[0].v} ${indices[0].n} ${indices[1].v} ${indices[1].n} ${indices[2].v} ${indices[2].n}`);
      }
    }
  }

  // Build output with counts first
  const output = [];
  output.push(`${vertices.length}`);
  output.push(`${normals.length}`);
  output.push(`${faces.length}`);
  output.push(...vertices);
  output.push(...normals);
  output.push(...faces);

  fs.writeFileSync(outputPath, output.join('\n'), 'utf-8');
  console.log(`Conversion complete. Output written to ${outputPath}`);
  console.log(`Vertices: ${vertices.length}, Normals: ${normals.length}, Faces: ${faces.length}`);
}

processObjFile(inputFile, outputFile);