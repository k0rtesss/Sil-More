/* Compose approved achievement artwork from shipped sprites into reserved
 * 64px atlas squares. Existing gameplay pixels are never changed.
 * Usage: node scripts/sync_achievement_icons.cjs [path-to-sharp]
 * Recipes: src/achievement-icon-compositions.json
 */
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const checkOnly = process.argv.includes('--check');
const sharp = require(process.argv.slice(2).find(arg=>arg!=='--check') || 'sharp');
const root = path.resolve(__dirname, '..');
const graf = path.join(root, 'lib/xtra/graf');
const recipes = JSON.parse(fs.readFileSync(path.join(root, 'src/achievement-icon-compositions.json')));
const atlasPath = path.join(graf, '16x16.png');
const sourcesPath = path.join(graf, 'achievement-icons.json');
const hash = data => crypto.createHash('sha256').update(data).digest('hex');
const cache = new Map();
let source, sourceInfo;

function region(data, info, x, y, width, height) {
  const result = Buffer.alloc(width * height * 4);
  for (let row = 0; row < height; ++row) {
    if (y + row >= info.height) continue;
    data.copy(result, row * width * 4,
      ((y + row) * info.width + x) * 4,
      ((y + row) * info.width + x + width) * 4);
  }
  return result;
}

async function sprite(name, crop = true) {
  const cacheKey = `${name}:${crop}`;
  if (cache.has(cacheKey)) return cache.get(cacheKey);
  const spec = recipes.sprites[name];
  assert(spec, `Unknown sprite ${name}`);
  let data;
  let info = { width:16, height:16, channels:4 };
  if (spec.file) {
    data = await sharp(path.join(graf, spec.file))
      .extract({left:0,top:0,width:16,height:16}).ensureAlpha().raw().toBuffer();
  } else {
    const [row,col] = spec.tile;
    assert(row * 16 + 16 <= recipes.atlas_first_row * 16,
      'Compositions must use original gameplay art, not another composition');
    data = region(source, sourceInfo, col * 16, row * 16, 16, 16);
  }
  if (spec.empty_jewel) {
    const [x,y,w,h] = spec.empty_jewel;
    for(let yy=y; yy<y+h; ++yy) for(let xx=x; xx<x+w; ++xx) {
      /* Replace the freed central jewel with the Iron Crown's dark metal.
       * The original three-jewel gameplay crown remains untouched. */
      const offset=(yy*16+xx)*4;
      data[offset]=57;data[offset+1]=65;data[offset+2]=66;data[offset+3]=255;
    }
  }
  if(crop) {
    let left=16,top=16,right=-1,bottom=-1;
    for(let y=0;y<16;++y) for(let x=0;x<16;++x) if(data[(y*16+x)*4+3]>20) {
      left=Math.min(left,x);top=Math.min(top,y);right=Math.max(right,x);bottom=Math.max(bottom,y);
    }
    assert(right>=left, `Empty source sprite ${name}`);
    info={width:right-left+1,height:bottom-top+1,channels:4};
    data=region(data,{width:16,height:16},left,top,info.width,info.height);
  }
  const value={data,info};cache.set(cacheKey,value);return value;
}

async function place(layers,name,cx,cy,maxWidth,maxHeight,crop=true,rotation=0) {
  const {data,info}=await sprite(name,crop);
  const scale=Math.min(maxWidth/info.width,maxHeight/info.height);
  const resized=sharp(data,{raw:info})
    .resize(Math.max(1,Math.floor(info.width*scale)),
      Math.max(1,Math.floor(info.height*scale)),{kernel:'nearest'});
  let image;
  if(rotation%90===0) image=await resized
    .rotate(rotation,{background:'#00000000'}).png().toBuffer();
  else {
    /* Keep diagonal poses pixel-sharp as well: sample the inverse rotation
     * with nearest neighbours instead of smoothing the original sprite. */
    const {data:pixels,info:size}=await resized.raw().toBuffer({resolveWithObject:true});
    const radians=rotation*Math.PI/180,c=Math.cos(radians),s=Math.sin(radians);
    const width=Math.ceil(Math.abs(size.width*c)+Math.abs(size.height*s)-1e-6);
    const height=Math.ceil(Math.abs(size.width*s)+Math.abs(size.height*c)-1e-6);
    const rotated=Buffer.alloc(width*height*4);
    for(let y=0;y<height;++y) for(let x=0;x<width;++x) {
      const sx=Math.floor(c*(x+.5-width/2)+s*(y+.5-height/2)+size.width/2);
      const sy=Math.floor(-s*(x+.5-width/2)+c*(y+.5-height/2)+size.height/2);
      if(sx>=0&&sx<size.width&&sy>=0&&sy<size.height)
        pixels.copy(rotated,(y*width+x)*4,(sy*size.width+sx)*4,(sy*size.width+sx)*4+4);
    }
    image=await sharp(rotated,{raw:{width,height,channels:4}}).png().toBuffer();
  }
  const dimensions=await sharp(image).metadata();
  const left=Math.round(cx-dimensions.width/2),top=Math.round(cy-dimensions.height/2);
  assert(left>=0&&top>=0&&left+dimensions.width<=64&&top+dimensions.height<=64,
    `Sprite outside composition: ${name}`);
  layers.push({input:image,left,top});
}

async function compose(icon) {
  const layers=[],a=icon.sprites;
  if(icon.layout==='single') await place(layers,a[0],32,32,48,48);
  else if(icon.layout==='pair') {
    await place(layers,a[0],23,32,36,44);
    await place(layers,a[1],48,38,24,28);
  } else if(icon.layout==='scene') {
    const background=await sprite(a[0],false);
    const input=await sharp(background.data,{raw:background.info})
      .resize(56,56,{kernel:'nearest'}).png().toBuffer();
    layers.push({input,left:4,top:4});
    await place(layers,a[1],33,30,32,40);
  } else if(icon.layout==='trio') {
    await place(layers,a[0],20,22,27,27);
    await place(layers,a[1],44,22,27,27);
    const badge=['NEITHER_VOICE_DIMINISHED','SEVEN_TIMES_HE_CRIED'].includes(icon.key);
    await place(layers,a[2],32,46,badge?20:27,badge?20:27);
  } else if(icon.layout==='interlock') {
    await place(layers,a[0],24,26,32,40);
    await place(layers,a[1],41,38,32,40);
  } else if(icon.layout==='fan') {
    await place(layers,'bow',18,32,24,48);
    await place(layers,'arrow',39,17,24,24);
    await place(layers,'arrow',45,32,24,24);
    await place(layers,'arrow',49,47,24,24);
  } else if(icon.layout==='crownJewels') {
    await place(layers,'crown',32,31,56,30);
    await place(layers,'jewel',18,48,16,16);
    await place(layers,'jewel',32,16,16,16);
    await place(layers,'jewel',46,48,16,16);
  } else if(icon.layout==='fall') {
    const chasm=await sprite('chasm',false),rubble=await sprite('rubble',false);
    const voidPixels=await sharp(chasm.data,{raw:chasm.info})
      .resize(64,64,{kernel:'nearest'}).raw().toBuffer();
    const rockPixels=await sharp(rubble.data,{raw:rubble.info})
      .resize(64,64,{kernel:'nearest'}).raw().toBuffer();
    const background=Buffer.alloc(64*64*4);
    /* Two broken stone ledges frame a broad, bottomless opening. Bright
     * lips and dark inner faces make the drop readable at small UI sizes. */
    const edge=[7,7,9,9,6,6,8,8,7,7,5,5,7,7,6,6];
    for(let y=4;y<62;++y) for(let x=4;x<60;++x) {
      const left=edge[Math.floor(y/4)]*2,right=64-edge[Math.floor((63-y)/4)]*2;
      const offset=(y*64+x)*4;
      let rgb=Array.from(voidPixels.subarray(offset,offset+3)).map(v=>Math.round(v*.12));
      if(x<left||x>=right) {
        const distance=x<left?left-1-x:x-right;
        rgb=Array.from(rockPixels.subarray(offset,offset+3)).map(v=>Math.min(120,Math.max(45,v)));
        if(distance<2) rgb=[131,138,125];
        else if(distance<4) rgb=[82,89,81];
      } else if(x<left+3||x>=right-3) rgb=[23,28,30];
      background[offset]=rgb[0];background[offset+1]=rgb[1];background[offset+2]=rgb[2];background[offset+3]=255;
    }
    layers.push({input:await sharp(background,{raw:{width:64,height:64,channels:4}})
      .png().toBuffer(),left:0,top:0});
    await place(layers,'traveller',32,31,29,46,true,135);
  } else if(icon.layout==='health') {
    await place(layers,'sword',32,26,44,44);
    /* Same frame/track/fill colours and inset as sdl_render_health_bar_rect.
     * The ten filled pixels are one quarter of the forty-pixel inner track. */
    const bar=Buffer.alloc(42*6*4);
    for(let y=0;y<6;++y) for(let x=0;x<42;++x) {
      const border=x===0||x===41||y===0||y===5;
      const rgb=border?[128,128,128]:x<=10?[192,0,0]:[80,80,80];
      const offset=(y*42+x)*4;bar[offset]=rgb[0];bar[offset+1]=rgb[1];bar[offset+2]=rgb[2];bar[offset+3]=255;
    }
    layers.push({input:await sharp(bar,{raw:{width:42,height:6,channels:4}}).png().toBuffer(),left:11,top:53});
  } else throw new Error(`Unknown layout ${icon.layout}`);
  return sharp({create:{width:64,height:64,channels:4,background:'#00000000'}})
    .composite(layers).raw().toBuffer();
}

(async()=>{
  assert.equal(recipes.size,64);
  const defines=fs.readFileSync(path.join(root,'src/defines.h'),'utf8');
  const quartzRow=Number(defines.match(/#define\s+GRAPHICS_QUARTZ_OVERLAY_ROW\s+(\d+)/)[1]);
  assert(recipes.atlas_first_row>quartzRow,
    'Achievement compositions must remain below the runtime quartz overlay');
  assert.equal(new Set(recipes.icons.map(i=>i.key)).size,recipes.icons.length);
  ({data:source,info:sourceInfo}=await sharp(atlasPath).ensureAlpha().raw().toBuffer({resolveWithObject:true}));
  assert.equal(sourceInfo.width,512);
  const old=fs.existsSync(sourcesPath)?JSON.parse(fs.readFileSync(sourcesPath)):null;
  const width=sourceInfo.width;
  const firstY=recipes.atlas_first_row*16;
  const height=Math.max(sourceInfo.height,firstY+Math.ceil(recipes.icons.length/8)*64);
  const next=Buffer.alloc(width*height*4);source.copy(next);
  const generated={size:64,atlas_first_row:recipes.atlas_first_row,sprites:recipes.sprites,icons:{}};
  let header=fs.readFileSync(path.join(root,'src/achievement-icons.h'),'utf8');
  const previousHeader=header;
  for(const [index,icon] of recipes.icons.entries()) {
    const x=(index%8)*64,y=firstY+Math.floor(index/8)*64;
    const existing=region(source,sourceInfo,x,y,64,64);
    const empty=Array.from({length:64*64},(_,i)=>existing[i*4+3]).every(a=>a===0);
    assert(empty||old?.icons[icon.key]?.rgba_sha256===hash(existing),
      `Refusing to overwrite unrecognised atlas artwork at ${y/16}:${x/16}`);
    const pixels=await compose(icon);
    if(checkOnly) assert(pixels.equals(existing), `Stale composed icon: ${icon.key}`);
    for(let row=0;row<64;++row) pixels.copy(next,((y+row)*width+x)*4,row*64*4,(row+1)*64*4);
    const pattern=new RegExp(`^SIL_ACHIEVEMENT_ICON(?:_RECT)?\\(${icon.key},[^\\n]*$`,'gm');
    const matches=header.match(pattern);assert.equal(matches?.length,1,icon.key);
    header=header.replace(pattern,`SIL_ACHIEVEMENT_ICON_RECT(${icon.key}, ${y/16}, ${x/16}, 64) /* ${icon.label} */`+(matches[0].endsWith('\r')?'\r':''));
    generated.icons[icon.key]={row:y/16,col:x/16,size:64,label:icon.label,layout:icon.layout,sprites:icon.sprites,rgba_sha256:hash(pixels)};
  }
  /* Check all pixels outside the reserved region, including the entire old
   * gameplay atlas, before writing either the image or its mappings. */
  assert(next.subarray(0,Math.min(firstY,sourceInfo.height)*width*4)
    .equals(source.subarray(0,Math.min(firstY,sourceInfo.height)*width*4)));
  if(checkOnly) {
    assert.equal(header,previousHeader,'Stale achievement atlas mappings');
    assert.deepEqual(generated,old,'Stale achievement source manifest');
    assert(next.equals(source),'Atlas differs from its generated compositions');
    console.log(`PASS: ${recipes.icons.length} composed icons, mappings and source manifest match the shipped atlas.`);
  } else {
    await sharp(next,{raw:{width,height,channels:4}}).png({compressionLevel:9}).toFile(atlasPath);
    if(header!==previousHeader)
      fs.writeFileSync(path.join(root,'src/achievement-icons.h'),header);
    fs.writeFileSync(sourcesPath,JSON.stringify(generated,null,2)+'\n');
    console.log(`PASS: composed ${recipes.icons.length} approved 64px icons; every existing gameplay pixel is unchanged.`);
  }
})().catch(error=>{console.error(error.stack);process.exitCode=1;});
