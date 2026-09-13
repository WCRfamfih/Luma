// Rebuild with: node tools/banner.cjs (requires sharp resolvable by Node).
// The product image is an unmodified native render.
const fs = require('node:fs');
const path = require('node:path');
const sharp = require('sharp');
const root = path.resolve(__dirname, '..');
const photo = fs.readFileSync(path.join(root,'docs/images/visualizer.png')).toString('base64');
const svg = `<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="1600" height="720" viewBox="0 0 1600 720">
<defs>
 <radialGradient id="air"><stop stop-color="#132a39"/><stop offset="1" stop-color="#090f14"/></radialGradient>
 <linearGradient id="rule"><stop stop-color="#74bfff"/><stop offset="1" stop-color="#74bfff" stop-opacity="0"/></linearGradient>
</defs>
<rect width="1600" height="720" fill="#090f14"/>
<ellipse cx="1190" cy="352" rx="620" ry="530" fill="url(#air)"/>
<path d="M80 82h44" stroke="#9de5cc" stroke-width="2"/>
<text x="143" y="88" fill="#9de5cc" font-family="Segoe UI, sans-serif" font-size="17" letter-spacing="4">DESKTOP AUDIO SCULPTURE</text>
<image x="807" y="23" width="690" height="690" xlink:href="data:image/png;base64,${photo}"/>
<text x="70" y="315" fill="#f0f7fb" font-family="Segoe UI Light, Segoe UI, sans-serif" font-weight="300" font-size="180" letter-spacing="-9">Luma</text>
<text x="85" y="391" fill="#c8ddea" font-family="Segoe UI, sans-serif" font-size="35" font-weight="300" letter-spacing=".2">Let sound take shape.</text>
<path d="M85 440h560" stroke="url(#rule)" stroke-opacity=".5"/>
<text x="85" y="488" fill="#8fa8b8" font-family="Microsoft YaHei, sans-serif" font-size="23" letter-spacing="3">声音的形状，桌面上的流光。</text>
<g font-family="Segoe UI, sans-serif" font-size="15" letter-spacing="2" fill="#a5b7c2">
 <text x="85" y="622">WINDOWS NATIVE</text><circle cx="280" cy="616" r="2" fill="#628598"/>
 <text x="300" y="622">UNDER 1 MB</text><circle cx="454" cy="616" r="2" fill="#628598"/>
 <text x="474" y="622">MIT OPEN SOURCE</text>
</g>
<text x="1517" y="671" text-anchor="end" fill="#607b8d" font-family="Segoe UI, sans-serif" font-size="14" letter-spacing="2">BY WCRfamfih</text>
</svg>`;
fs.writeFileSync(path.join(root,'docs/images/banner.svg'),svg);
sharp(Buffer.from(svg)).png().toFile(path.join(root,'docs/images/banner.png')).then(()=>console.log('Rendered 1600 x 720 banner.'));
