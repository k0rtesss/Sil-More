/* Local authoring UI. Source images and projects never leave this browser. */
'use strict';
(() => {
  const $=id=>document.getElementById(id), E=TileForge, X=ForgeExport;
  const slots=['foreground','background','face'], knobs=['inset','roughness','blend','rim','depth'];
  const state={size:16,mode:'floor',sources:[],tiles:{},grid:[],brush:1,mask:0,view:'map',history:[],cache:new Map(),overrides:{},mixedMaterials:[],mixedGrid:[],mixedBrush:0,mixedHistory:[]};
  let picking='foreground',pick={source:0,col:0,row:0},painting=false,lastCell=null,queued=false;
  const names={floor:['Upper floor','Lower floor','Floor transition'],wall:['Wall top','Ground floor','Walls with depth'],chasm:['Floor surface','Chasm / void','Edges above the dark']};
  function status(message,error=false){$('status').textContent=message;$('status').style.color=error?'#efb18d':'';}
  function guarded(fn){return async(...args)=>{try{await fn(...args);}catch(error){status(error.message,true);console.error(error);}};}
  function canvas(width,height=width){const c=document.createElement('canvas');c.width=width;c.height=height;return c;}
  function put(target,pixels){target.width=pixels.width;target.height=pixels.height;target.getContext('2d').putImageData(new ImageData(new Uint8ClampedArray(pixels.data),pixels.width,pixels.height),0,0);}
  function options(){return {size:state.size,mode:state.mode,style:$('edge-style').value,inset:+$('inset').value,roughness:+$('roughness').value,blend:+$('blend').value,rim:+$('rim').value,depth:+$('depth').value,seed:Math.max(0,Math.min(999999,Math.trunc(+$('seed').value)||0)),paletteLock:$('palette-lock').checked,deriveFace:$('derive-face').checked};}
  function pixelsFor(slot){
    const tile=typeof slot==='object'?slot:state.tiles[slot],source=state.sources.find(s=>s.id===tile?.source);
    if(!source) return null;
    const c=canvas(state.size),ctx=c.getContext('2d',{willReadFrequently:true});
    ctx.imageSmoothingEnabled=false;ctx.drawImage(source.image,tile.col*state.size,tile.row*state.size,state.size,state.size,0,0,state.size,state.size);
    return ctx.getImageData(0,0,state.size,state.size);
  }
  function tile(mask){
    mask=E.normalizeMask(mask);
    if(!state.cache.has(mask)){
      const c=canvas(state.size);
      const override=state.overrides[`${state.mode}:${mask}`];
      if(override)c.getContext('2d').drawImage(override.image,0,0);
      else put(c,E.renderTile({...options(),mask,foreground:pixelsFor('foreground'),background:pixelsFor('background'),face:$('derive-face').checked?null:pixelsFor('face')}));
      state.cache.set(mask,c);
    }
    return state.cache.get(mask);
  }
  function ready(){return slots.every(s=>state.tiles[s]&&pixelsFor(s));}
  function updateSwatches(){
    for(const slot of slots){
      const p=pixelsFor(slot);if(p)put($(`swatch-${slot}`),p);
      const ref=state.tiles[slot],src=state.sources.find(s=>s.id===ref?.source);
      $(`source-${slot}`).textContent=ref?`${src?.name} · ${ref.col}, ${ref.row}`:'Choose a tile';
      $(`source-${slot}`).title=ref?`${src?.name}: column ${ref.col}, row ${ref.row}`:'';
    }
    if($('derive-face').checked){put($('swatch-face'),pixelsFor('foreground'));$('source-face').textContent='Derived from upper tile';}
  }
  function refresh(){
    state.cache.clear();
    for(const id of knobs)document.querySelector(`output[for="${id}"]`).textContent=$(`${id}`).value+' px';
    const labels=names[state.mode];$('label-foreground').textContent=labels[0];$('label-background').textContent=labels[1];
    document.querySelectorAll('[data-mode]').forEach(b=>b.classList.toggle('active',b.dataset.mode===state.mode));
    $('size-badge').textContent=`${state.size} × ${state.size} PX`;
    $('rim').disabled=state.mode==='floor'&&state.view!=='mixed';$('depth').disabled=state.mode==='floor'&&state.view!=='mixed';
    $('blend').disabled=state.view!=='mixed'&&!['dither','soft'].includes($('edge-style').value);
    $('inset').disabled=state.view==='mixed';
    $('roughness').disabled=$('edge-style').value==='cut';
    $('mode-caption').textContent=state.mode==='chasm'?'Upper = solid ground. Lower = the drop.':state.mode==='wall'?'Upper = wall top. Face descends toward the screen.':'Upper material connects to matching neighbors.';
    $('tip-title').textContent=state.mode==='chasm'?'A ledge needs a silhouette.':state.mode==='wall'?'Give the wall a front.':'Let the drawing lead.';
    $('tip-text').textContent=state.mode==='chasm'?'Start with a one-pixel lip and a dark face. Increase inset to leave space for depth. Keep a clear difference between walkable floor and void.':state.mode==='wall'?'Choose a darker face tile than the top. Face depth is constrained by the available space inside each tile. Try the rooms scene to inspect corners.':'Use a narrow transition for detailed stone. Dither is useful when two floors have very different colors.';
    $('empty-state').hidden=ready();
    for(const id of ['export-pack','export-atlas','export-tile','save-project'])$(id).disabled=!ready();
    if(!ready())return;
    updateSwatches();drawMixedPalette();drawMap();drawGallery();drawDetail();
  }
  function schedule(){if(!queued){queued=true;requestAnimationFrame(()=>{queued=false;guarded(refresh)();});}}
  function createScene(kind){
    const w=20,h=13;
    if(kind==='rooms')return Array.from({length:h},(_,y)=>Array.from({length:w},(_,x)=>x===1||x===w-2||y===1||y===h-2||x===9&&y!==5&&y!==6||y===7&&x>9&&x!==14?1:0));
    if(kind==='islands')return Array.from({length:h},(_,y)=>Array.from({length:w},(_,x)=>{
      if(x===2&&y===2||x>=5&&x<=8&&y>=2&&y<=5||x>=12&&x<=17&&y===2||x===14&&y>=2&&y<=5)return 1;
      if(x>=2&&x<=10&&y>=8&&y<=10)return !(x>=5&&x<=7&&y===9)?1:0;
      return x>=13&&x<=17&&y>=8&&y<=11&&(x<=14||y>=10)?1:0;
    }));
    return Array.from({length:h},(_,y)=>Array.from({length:w},(_,x)=>{
      const left=(x-5.7)**2/33+(y-6.4)**2/25<1,right=(x-14.4)**2/19+(y-6)**2/15<1;
      const bridge=x>=8&&x<=13&&y>=5&&y<=7;
      const hole=x>=4&&x<=5&&y>=5&&y<=6;
      return (left||right||bridge)&&!hole?1:0;
    }));
  }
  function makeMap(repeat=false){
    const grid=state.grid,s=state.size;
    const c=canvas((repeat?6:grid[0].length)*s,(repeat?6:grid.length)*s),ctx=c.getContext('2d');ctx.imageSmoothingEnabled=false;
    const bg=canvas(s);put(bg,pixelsFor('background'));
    for(let y=0;y<c.height/s;y++)for(let x=0;x<c.width/s;x++){
      ctx.drawImage(repeat?tile(state.mask):grid[y][x]?tile(E.maskAt(grid,x,y)):bg,x*s,y*s);
    }
    return c;
  }
  function mixedScene(){
    const count=state.mixedMaterials.length;
    if(!count)return Array.from({length:13},()=>Array(20).fill(0));
    const chasm=state.mixedMaterials.findIndex(m=>m.kind==='chasm');
    return Array.from({length:13},(_,y)=>Array.from({length:20},(_,x)=>{
      if(chasm>=0&&(y===0||y===12||x===0||x===19||x>=8&&x<=10&&y>=5&&y<=7||x===7&&y===6))return chasm;
      return (Math.floor(x/4)+Math.floor(y/4)*2) % count;
    }));
  }
  function defaultMixed(){
    const src=state.sources.find(s=>s.id==='sil-more')||state.sources[0];if(!src)return;
    const defs=src.id==='sil-more'&&state.size===16?[
      ['Original floor','floor',1,0],['Stone','floor',16,36],['Earth','floor',8,36],['Snow','floor',0,35],
      ['Original wall','wall',4,0],['Masonry','wall',0,33],['Rock wall','wall',2,33],['Chasm','chasm',0,34]
    ]:[['Floor A','floor',0,0],['Floor B','floor',1,0],['Wall','wall',2,0]];
    state.mixedMaterials=defs.map(([name,kind,col,row])=>({name,kind,...tileRef(src,col,row)}));
    state.mixedGrid=mixedScene();state.mixedBrush=0;state.mixedHistory=[];
  }
  function makeMixed(){
    const c=canvas(20*state.size,13*state.size);
    if(state.mixedMaterials.length)put(c,ForgeMaterials.renderMap({...options(),grid:state.mixedGrid,materials:state.mixedMaterials.map(m=>({...m,pixels:pixelsFor(m)}))}));
    return c;
  }
  function drawMixedPalette(){
    $('mixed-palette').replaceChildren(...state.mixedMaterials.map((m,i)=>{
      const b=document.createElement('button'),swatch=canvas(state.size);put(swatch,pixelsFor(m));
      b.className='mixed-brush'+(state.mixedBrush===i?' active':'');b.setAttribute('aria-label',`Paint ${m.name}`);b.setAttribute('aria-pressed',state.mixedBrush===i);
      b.append(swatch,Object.assign(document.createElement('span'),{textContent:m.name}));b.title=`${m.kind} · ${m.col}, ${m.row}`;
      b.onclick=()=>{state.mixedBrush=i;drawMixedPalette();};return b;
    }));
    $('mixed-kind').value=state.mixedMaterials[state.mixedBrush]?.kind||'floor';
    $('mixed-name').value=state.mixedMaterials[state.mixedBrush]?.name||'';
    $('mixed-add').disabled=state.mixedMaterials.length>=32;
  }
  function drawMap(){
    if(!ready())return;
    const repeat=state.view==='repeat',mixed=state.view==='mixed',c=mixed?makeMixed():makeMap(repeat),target=$('map'),ctx=target.getContext('2d');
    target.width=c.width;target.height=c.height;ctx.imageSmoothingEnabled=false;ctx.drawImage(c,0,0);
    if($('show-grid').checked){ctx.strokeStyle='#d2e7b23d';ctx.lineWidth=1;ctx.beginPath();for(let x=0;x<c.width;x+=state.size){ctx.moveTo(x+.5,0);ctx.lineTo(x+.5,c.height);}for(let y=0;y<c.height;y+=state.size){ctx.moveTo(0,y+.5);ctx.lineTo(c.width,y+.5);}ctx.stroke();}
    const zoom=+$('zoom').value;target.style.width=c.width*zoom+'px';target.style.height=c.height*zoom+'px';
    $('undo').disabled=!(mixed?state.mixedHistory:state.history).length||repeat;
    $('mixed-tools').hidden=!mixed;$('brush-upper').hidden=mixed;$('brush-lower').hidden=mixed;$('scene').disabled=mixed;
    $('map').setAttribute('aria-label',mixed?'Paintable mixed terrain preview; choose a material brush':'Paintable terrain preview; use the upper and lower brushes');
    document.querySelector('.atlas-hint').textContent=mixed?'The 47 shapes below belong to the upper/lower material pair. Export pack also includes the mixed scene PNG and its material map.':'Select a shape to inspect it. All 256 neighbor combinations resolve to these 47 tiles.';
    if(mixed)$('mode-caption').textContent='All floor / wall pairings connect. Chasm rims apply to every solid material.';
  }
  function drawGallery(){
    const gallery=$('tile-gallery');
    if(!gallery.children.length)for(const mask of E.MASKS){
      const b=document.createElement('button');b.className='tile-button';b.dataset.mask=mask;b.title=`Inspect mask ${mask}`;b.setAttribute('aria-label',`Inspect mask ${mask}`);
      b.append(canvas(state.size),Object.assign(document.createElement('span'),{textContent:String(mask).padStart(3,'0')}));
      b.onclick=()=>{state.mask=mask;drawDetail();if(state.view==='repeat')drawMap();};gallery.append(b);
    }
    for(const b of gallery.children){const c=b.firstChild;c.width=c.height=state.size;c.getContext('2d').drawImage(tile(+b.dataset.mask),0,0);b.classList.toggle('selected',+b.dataset.mask===E.normalizeMask(state.mask));}
  }
  function drawDetail(){
    if(!ready())return;
    const m=E.normalizeMask(state.mask),index=E.MASKS.indexOf(m),target=$('detail');target.width=target.height=state.size;target.getContext('2d').drawImage(tile(m),0,0);
    $('mask-number').textContent='#'+String(m).padStart(3,'0');$('raw-mask').textContent=state.mask;$('normal-mask').textContent=m;$('sheet-position').textContent=`${index%8}, ${Math.floor(index/8)}`;
    document.querySelectorAll('[data-bit]').forEach(b=>{b.classList.toggle('active',!!(state.mask&+b.dataset.bit));b.setAttribute('aria-pressed',!!(state.mask&+b.dataset.bit));});
    document.querySelectorAll('.tile-button').forEach(b=>b.classList.toggle('selected',+b.dataset.mask===m));
    const custom=!!state.overrides[`${state.mode}:${m}`];$('clear-override').hidden=!custom;
    $('override-status').textContent=custom?'Hand-drawn replacement active for this shape.':'Fine-tune any shape in your drawing app.';
  }
  function imageFromURL(url){return new Promise((resolve,reject)=>{const im=new Image();im.onload=()=>resolve(im);im.onerror=()=>reject(new Error('Could not decode this PNG.'));im.src=url;});}
  function readFile(file){return new Promise((resolve,reject)=>{const r=new FileReader();r.onload=()=>resolve(r.result);r.onerror=()=>reject(new Error('Could not read file.'));r.readAsDataURL(file);});}
  async function requirePNG(file){const signature=new Uint8Array(await file.slice(0,8).arrayBuffer());if(![137,80,78,71,13,10,26,10].every((v,i)=>signature[i]===v))throw new Error('Choose a PNG image; this file has a different format.');}
  async function sourceFrom(url,name,id){
    const image=await imageFromURL(url);
    if(image.width>4096||image.height>4096||image.width<8||image.height<8)throw new Error('Source images must be between 8 and 4096 pixels on each side.');
    const c=canvas(image.width,image.height);c.getContext('2d').drawImage(image,0,0);
    return {id:id||crypto.randomUUID(),name,image,dataUrl:c.toDataURL('image/png')};
  }
  function tileRef(source,col,row){return {source:source.id,col:Math.max(0,Math.min(col,Math.floor(source.image.width/state.size)-1)),row:Math.max(0,Math.min(row,Math.floor(source.image.height/state.size)-1))};}
  function assignDefaults(source){slots.forEach((slot,i)=>state.tiles[slot]=tileRef(source,i,0));}
  function applyPreset(value){
    const src=state.sources.find(s=>s.id==='sil-more');
    const defs={earth:{mode:'floor',coords:[[16,36],[8,36],[2,33]],style:'organic',inset:3,roughness:.75,blend:1,rim:0,depth:4,title:'Stone meets earth'},frost:{mode:'floor',coords:[[0,35],[16,36],[2,33]],style:'dither',inset:3,roughness:.5,blend:2,rim:0,depth:4,title:'Snow settles on stone'},wall:{mode:'wall',coords:[[0,33],[16,36],[2,33]],style:'cut',inset:4,roughness:0,blend:0,rim:1,depth:4,title:'Masonry, with dimension'},chasm:{mode:'chasm',coords:[[16,36],[0,34],[3,33]],style:'organic',inset:4,roughness:.75,blend:0,rim:1,depth:4,title:'A floor above the abyss'}};
    const p=defs[value];state.mode=p.mode;$('stage-title').textContent=p.title;$('edge-style').value=p.style;
    $('derive-face').checked=value!=='wall';
    for(const k of knobs)$(k).value=p[k]*state.size/16;
    if(src&&state.size===16)slots.forEach((slot,i)=>state.tiles[slot]=tileRef(src,...p.coords[i]));
    if(value==='wall'){$('scene').value='rooms';state.grid=createScene('rooms');state.history=[];}
    else{$('scene').value='cavern';state.grid=createScene('cavern');state.history=[];}
    refresh();status('Starting point applied. Choose any source tiles to make it yours.');
  }
  function openPicker(slot){
    if(!state.sources.length){$('png-file').click();return;}
    picking=slot;const ref=slot==='mixed-edit'?state.mixedMaterials[state.mixedBrush]:state.tiles[slot];pick={source:Math.max(0,state.sources.findIndex(s=>s.id===ref?.source)),col:ref?.col||0,row:ref?.row||0};
    $('picker-title').textContent=slot.startsWith('mixed-')?'Choose material for the mixed palette':'Choose '+(slot==='face'?'cliff / wall face':names[state.mode][slot==='foreground'?0:1].toLowerCase());
    $('source-select').replaceChildren(...state.sources.map((s,i)=>Object.assign(document.createElement('option'),{value:i,textContent:s.name})));
    $('source-select').value=pick.source;drawPicker();$('picker').showModal();
  }
  function drawPicker(){
    const src=state.sources[pick.source],z=+$('atlas-zoom').value,s=state.size;
    const cols=Math.floor(src.image.width/s),rows=Math.floor(src.image.height/s);
    pick.col=Math.max(0,Math.min(pick.col,cols-1));pick.row=Math.max(0,Math.min(pick.row,rows-1));
    $('pick-col').value=pick.col;$('pick-row').value=pick.row;$('pick-col').max=cols-1;$('pick-row').max=rows-1;
    const target=$('source-atlas');target.width=src.image.width*z;target.height=src.image.height*z;
    const ctx=target.getContext('2d');ctx.imageSmoothingEnabled=false;ctx.drawImage(src.image,0,0,target.width,target.height);
    ctx.strokeStyle='#d5e6c12b';ctx.lineWidth=1;ctx.beginPath();for(let x=0;x<=cols;x++){ctx.moveTo(x*s*z+.5,0);ctx.lineTo(x*s*z+.5,rows*s*z);}for(let y=0;y<=rows;y++){ctx.moveTo(0,y*s*z+.5);ctx.lineTo(cols*s*z,y*s*z+.5);}ctx.stroke();
    ctx.strokeStyle='#e2ffa3';ctx.lineWidth=2;ctx.strokeRect(pick.col*s*z+1,pick.row*s*z+1,s*z-2,s*z-2);
    $('pick-info').textContent=`${src.image.width} × ${src.image.height} px · ${cols} columns × ${rows} rows · Selected ${pick.col}, ${pick.row}`;
  }
  function brush(e){
    if(!painting||state.view==='repeat')return;
    const grid=state.view==='mixed'?state.mixedGrid:state.grid;
    const r=$('map').getBoundingClientRect(),x=Math.floor((e.clientX-r.left)/r.width*state.grid[0].length),y=Math.floor((e.clientY-r.top)/r.height*state.grid.length);
    if(x<0||y<0||x>=state.grid[0].length||y>=state.grid.length)return;
    if(lastCell&&lastCell.x===x&&lastCell.y===y)return;
    const from=lastCell||{x,y},steps=Math.max(Math.abs(x-from.x),Math.abs(y-from.y),1);
    for(let i=0;i<=steps;i++)grid[Math.round(from.y+(y-from.y)*i/steps)][Math.round(from.x+(x-from.x)*i/steps)]=e.buttons===2?0:state.view==='mixed'?state.mixedBrush:state.brush;
    lastCell={x,y};drawMap();
  }
  function atlas(raw=false){const masks=raw?Array.from({length:256},(_,m)=>m):E.MASKS,cols=raw?16:8,s=state.size,c=canvas(cols*s,Math.ceil(masks.length/cols)*s),ctx=c.getContext('2d');masks.forEach((m,i)=>ctx.drawImage(tile(m),i%cols*s,Math.floor(i/cols)*s));return c;}
  function metadata(){
    return {format:'sil-more-tile-forge',version:1,tileSize:state.size,parameters:options(),sources:Object.fromEntries(slots.map(slot=>[slot,{...state.tiles[slot],name:state.sources.find(s=>s.id===state.tiles[slot].source)?.name}])),directionBits:{N:1,NE:2,E:4,SE:8,S:16,SW:32,W:64,NW:128},maskMeaning:'Matching upper-material neighbors. Diagonals require both adjoining cardinals. In chasm mode the center is solid floor, NOT a chasm cell.',compact:{file:'atlas-47.png',columns:8,rows:6,count:47},expanded:{file:'atlas-256.png',columns:16,rows:16,count:256},tiles:E.MASKS.map((mask,index)=>({index,mask,col:index%8,row:Math.floor(index/8),x:index%8*state.size,y:Math.floor(index/8)*state.size,override:!!state.overrides[`${state.mode}:${mask}`]})),rawToCanonical:Array.from({length:256},(_,m)=>E.normalizeMask(m)),rawToIndex:Array.from({length:256},(_,m)=>E.MASKS.indexOf(E.normalizeMask(m)))};
  }
  function tsx(meta){return `<?xml version="1.0" encoding="UTF-8"?>\n<tileset version="1.10" name="Tile Forge" tilewidth="${state.size}" tileheight="${state.size}" tilecount="47" columns="8"><image source="atlas-47.png" width="${8*state.size}" height="${6*state.size}"/>${meta.tiles.map(t=>`<tile id="${t.index}"><properties><property name="mask" type="int" value="${t.mask}"/><property name="mode" value="${state.mode}"/></properties></tile>`).join('')}</tileset>`;}
  async function exportPack(){
    if(!ready())return;const meta=metadata(),prefix=`tile-forge-${state.mode}-${state.size}px`;
    status('Packing atlas, individual tiles, and preview…');$('export-pack').disabled=true;
    try{
      const pending=[{name:'atlas-47.png',data:X.png(atlas())},{name:'atlas-256.png',data:X.png(atlas(true))},{name:'preview.png',data:X.png(makeMap())},{name:'tiles.json',data:JSON.stringify(meta,null,2)},{name:'tileset.tsx',data:tsx(meta)},{name:'README.txt',data:'Tile Forge export\n\nPNG pixels are native resolution, without preview grid lines.\nCompact sheet: 8 columns, 47 tiles; last cell is unused transparent padding.\nExpanded sheet: 16 x 16 raw neighbor-mask slots; canonical duplicates are intentional.\nClockwise bits: N=1 NE=2 E=4 SE=8 S=16 SW=32 W=64 NW=128.\nCenter and matching neighbors mean UPPER material. Chasm exports are FLOOR-centered.\nA Tiled atlas descriptor is included; terrain brush/Wang definitions are not configured.\nEngine mapping and material priority must be added explicitly. No game files were changed.\nSources remain subject to their original licenses.\n'}];
      if(state.mixedMaterials.length){pending.push({name:'mixed-preview.png',data:X.png(makeMixed())},{name:'mixed-scene.json',data:JSON.stringify({tileSize:state.size,materials:state.mixedMaterials,grid:state.mixedGrid,parameters:options()},null,2)});}
      for(const m of E.MASKS)pending.push({name:`tiles/mask-${String(m).padStart(3,'0')}.png`,data:X.png(tile(m))});
      const files=await Promise.all(pending.map(async f=>({...f,data:await f.data})));
      await X.download(await X.zip(files),prefix+'.zip');status('Pack ready: 47 tiles, both atlases, metadata, and preview. Download link below.');
    }finally{$('export-pack').disabled=false;}
  }
  function project(){return {format:'sil-more-tile-forge-project',version:1,parameters:options(),sources:state.sources.map(({id,name,dataUrl})=>({id,name,dataUrl})),tiles:state.tiles,grid:state.grid,mixedMaterials:state.mixedMaterials,mixedGrid:state.mixedGrid,mask:state.mask,scene:$('scene').value,title:$('stage-title').textContent,overrides:Object.fromEntries(Object.entries(state.overrides).map(([key,value])=>[key,value.dataUrl]))};}
  async function loadProject(file){
    if(file.size>40*1024*1024)throw new Error('Project exceeds 40 MB.');
    const p=JSON.parse(await file.text()),o=p.parameters;
    if(p.format!=='sil-more-tile-forge-project'||p.version!==1||!o||![8,16,24,32,48,64].includes(o.size)||!['floor','wall','chasm'].includes(o.mode)||!['organic','cut','dither','soft'].includes(o.style))throw new Error('Unrecognized Tile Forge project.');
    const limits={inset:[1,Math.floor(o.size/2)-1,1],roughness:[0,Math.floor(o.size/5),.25],blend:[0,Math.floor(o.size/3),.5],rim:[0,Math.floor(o.size/4),1],depth:[0,Math.floor(o.size/2)-1,1],seed:[0,999999,1]};
    for(const [k,[lo,hi,step]] of Object.entries(limits))if(!Number.isFinite(o[k])||o[k]<lo||o[k]>hi||Math.abs(o[k]/step-Math.round(o[k]/step))>1e-6)throw new Error(`Invalid project setting: ${k}.`);
    if(!Array.isArray(p.sources)||!p.sources.length||p.sources.length>12||new Set(p.sources.map(s=>s.id)).size!==p.sources.length)throw new Error('Project needs 1–12 uniquely identified source images.');
    const sources=await Promise.all(p.sources.map(s=>{if(typeof s.name!=='string'||typeof s.id!=='string'||!/^data:image\/png;base64,/.test(s.dataUrl))throw new Error('Project contains an invalid source.');return sourceFrom(s.dataUrl,s.name,s.id);}));
    for(const slot of slots){const t=p.tiles?.[slot],src=sources.find(s=>s.id===t?.source);if(!src||!Number.isInteger(t.col)||!Number.isInteger(t.row)||t.col<0||t.row<0||(t.col+1)*o.size>src.image.width||(t.row+1)*o.size>src.image.height)throw new Error(`Invalid ${slot} tile coordinates.`);}
    if(!Array.isArray(p.grid)||p.grid.length!==13||p.grid.some(row=>!Array.isArray(row)||row.length!==20||row.some(v=>v!==0&&v!==1)))throw new Error('Invalid painted scene.');
    if(p.mixedMaterials!==undefined){
      if(!Array.isArray(p.mixedMaterials)||!p.mixedMaterials.length||p.mixedMaterials.length>32)throw new Error('Mixed palette needs 1–32 materials.');
      for(const m of p.mixedMaterials){const src=sources.find(s=>s.id===m.source);if(!src||typeof m.name!=='string'||!['floor','wall','chasm'].includes(m.kind)||!Number.isInteger(m.col)||!Number.isInteger(m.row)||m.col<0||m.row<0||(m.col+1)*o.size>src.image.width||(m.row+1)*o.size>src.image.height)throw new Error('Invalid mixed material.');}
      if(!Array.isArray(p.mixedGrid)||p.mixedGrid.length!==13||p.mixedGrid.some(r=>!Array.isArray(r)||r.length!==20||r.some(v=>!Number.isInteger(v)||v<0||v>=p.mixedMaterials.length)))throw new Error('Invalid mixed scene.');
    }
    const overrides={};for(const [key,url] of Object.entries(p.overrides||{})){if(!/^(floor|wall|chasm):\d+$/.test(key)||!E.MASKS.includes(+key.split(':')[1])||!/^data:image\/png;base64,/.test(url))throw new Error('Invalid tile override.');const im=await imageFromURL(url);if(im.width!==o.size||im.height!==o.size)throw new Error('Override dimensions do not match the tile size.');overrides[key]={image:im,dataUrl:url};}
    Object.assign(state,{sources,size:o.size,mode:o.mode,tiles:p.tiles,grid:p.grid,mask:Number.isInteger(p.mask)?p.mask&255:0,history:[],overrides});
    if(p.mixedMaterials)Object.assign(state,{mixedMaterials:p.mixedMaterials,mixedGrid:p.mixedGrid,mixedBrush:0,mixedHistory:[]});else defaultMixed();
    $('tile-size').value=o.size;$('preset').value='custom';setLimits();for(const k of knobs)$(k).value=o[k];$('seed').value=o.seed;$('edge-style').value=o.style;$('palette-lock').checked=!!o.paletteLock;$('derive-face').checked=!!o.deriveFace;$('scene').value=['cavern','islands','rooms'].includes(p.scene)?p.scene:'cavern';$('stage-title').textContent=typeof p.title==='string'?p.title:names[o.mode][2];
    refresh();status(`Restored project with ${sources.length} source image(s).`);
  }
  function setLimits(){const s=state.size;$('inset').max=Math.floor(s/2)-1;$('roughness').max=Math.floor(s/5);$('blend').max=Math.floor(s/3);$('rim').max=Math.floor(s/4);$('depth').max=Math.floor(s/2)-1;}

  document.querySelectorAll('[data-slot]').forEach(b=>b.onclick=()=>openPicker(b.dataset.slot));
  document.querySelectorAll('[data-mode]').forEach(b=>b.onclick=()=>{state.mode=b.dataset.mode;$('preset').value='custom';$('stage-title').textContent=names[state.mode][2];refresh();});
  for(const k of [...knobs,'edge-style','seed','palette-lock','derive-face'])$(k).addEventListener('input',()=>{$('preset').value='custom';schedule();});
  $('new-seed').onclick=()=>{$('seed').value=(+ $('seed').value+7919)%1000000;schedule();};
  $('preset').onchange=()=>applyPreset($('preset').value);
  $('swap').onclick=()=>{[state.tiles.foreground,state.tiles.background]=[state.tiles.background,state.tiles.foreground];refresh();};
  $('import-png').onclick=()=>$('png-file').click();
  $('png-file').onchange=guarded(async()=>{
    const files=Array.from($('png-file').files);$('png-file').value='';if(!files.length)return;if(state.sources.length+files.length>12)throw new Error('Keep at most 12 source images in a project.');
    const additions=[];for(const f of files){if(f.size>16*1024*1024)throw new Error('Choose PNG files up to 16 MB.');await requirePNG(f);const source=await sourceFrom(await readFile(f),f.name);if(source.image.width<state.size||source.image.height<state.size)throw new Error('PNG is smaller than the selected tile size.');additions.push(source);}
    if([...state.sources,...additions].reduce((n,s)=>n+s.dataUrl.length,0)>36*1024*1024)throw new Error('Embedded artwork exceeds 36 MB. Use smaller atlases so the saved project can be reopened.');
    state.sources.push(...additions);if(!state.tiles.foreground)assignDefaults(additions[0]);if(!state.mixedMaterials.length)defaultMixed();refresh();status(`Imported ${additions.length} PNG(s). Select a material card to choose a cell.`);if(additions.length)openPicker('foreground');
    if(additions.length){pick={source:state.sources.length-additions.length,col:0,row:0};$('source-select').value=pick.source;drawPicker();}
  });
  $('tile-size').onchange=()=>{const next=+$('tile-size').value;if(state.sources.some(s=>s.image.width<next||s.image.height<next)){$('tile-size').value=state.size;status('A loaded source is smaller than that tile size.',true);return;}state.size=next;state.overrides={};setLimits();for(const slot of slots){const ref=state.tiles[slot];if(ref)state.tiles[slot]=tileRef(state.sources.find(s=>s.id===ref.source),ref.col,ref.row);}state.mixedMaterials=state.mixedMaterials.map(m=>({...m,...tileRef(state.sources.find(s=>s.id===m.source),m.col,m.row)}));refresh();status('Tile size changed. Source selections were clamped to the new grid; overrides cleared.');};
  $('source-select').onchange=()=>{pick.source=+$('source-select').value;pick.col=pick.row=0;drawPicker();};
  $('atlas-zoom').onchange=drawPicker;
  for(const axis of ['col','row'])$('pick-'+axis).onchange=()=>{pick[axis]=Math.floor(+$('pick-'+axis).value)||0;drawPicker();};
  $('source-atlas').onclick=e=>{const r=e.currentTarget.getBoundingClientRect(),z=+$('atlas-zoom').value;pick.col=Math.floor((e.clientX-r.left)/(state.size*z));pick.row=Math.floor((e.clientY-r.top)/(state.size*z));drawPicker();};
  $('source-atlas').onkeydown=e=>{const delta={ArrowLeft:[-1,0],ArrowRight:[1,0],ArrowUp:[0,-1],ArrowDown:[0,1]}[e.key];if(delta){e.preventDefault();pick.col+=delta[0];pick.row+=delta[1];drawPicker();}if(e.key==='Enter')$('use-tile').click();};
  $('use-tile').onclick=()=>{
    pick.col=Math.floor(+$('pick-col').value)||0;pick.row=Math.floor(+$('pick-row').value)||0;
    const ref=tileRef(state.sources[pick.source],pick.col,pick.row);
    if(picking.startsWith('mixed-')){
      if(picking==='mixed-add'){
        const kind=$('mixed-kind').value;
        state.mixedMaterials.push({...ref,kind,name:`${kind[0].toUpperCase()+kind.slice(1)} ${state.mixedMaterials.length+1} · ${ref.col},${ref.row}`});state.mixedBrush=state.mixedMaterials.length-1;
      }else Object.assign(state.mixedMaterials[state.mixedBrush],ref,{name:`${$('mixed-kind').value} · ${ref.col},${ref.row}`});
    }else{state.tiles[picking]=ref;if(picking==='face')$('derive-face').checked=false;}
    $('preset').value='custom';$('picker').close();refresh();status('Source tile updated.');
  };
  $('mixed-add').onclick=()=>openPicker('mixed-add');$('mixed-edit').onclick=()=>openPicker('mixed-edit');
  $('mixed-kind').onchange=()=>{const m=state.mixedMaterials[state.mixedBrush];if(m)m.kind=$('mixed-kind').value;drawMixedPalette();drawMap();};
  $('mixed-name').oninput=()=>{const m=state.mixedMaterials[state.mixedBrush];if(m){m.name=$('mixed-name').value.trim().slice(0,64)||'Material '+(state.mixedBrush+1);const b=$('mixed-palette').children[state.mixedBrush];b.lastChild.textContent=m.name;b.setAttribute('aria-label',`Paint ${m.name}`);}};
  $('mixed-export').onclick=guarded(async()=>X.download(await X.png(makeMixed()),'tile-forge-mixed-scene.png'));
  $('picker-close').onclick=()=>$('picker').close();$('help-open').onclick=()=>$('help').showModal();$('help-close').onclick=()=>$('help').close();
  const neighborLayout=[['NW',128],['N',1],['NE',2],['W',64],['',0],['E',4],['SW',32],['S',16],['SE',8]];
  for(const [name,bit] of neighborLayout){const b=document.createElement(bit?'button':'span');if(bit){b.textContent=name;b.dataset.bit=bit;b.setAttribute('aria-label','Toggle '+name+' neighbor');b.onclick=()=>{state.mask^=bit;drawDetail();if(state.view==='repeat')drawMap();};}else{b.className='center-cell';b.textContent='●';}$('neighbors').append(b);}
  $('map').onpointerdown=e=>{if(state.view==='repeat'||!ready())return;e.preventDefault();const history=state.view==='mixed'?state.mixedHistory:state.history,grid=state.view==='mixed'?state.mixedGrid:state.grid;history.push(grid.map(r=>r.slice()));if(history.length>40)history.shift();painting=true;lastCell=null;$('map').setPointerCapture(e.pointerId);brush(e);};
  $('map').onpointermove=brush;$('map').onpointerup=()=>painting=false;$('map').onpointercancel=()=>painting=false;$('map').oncontextmenu=e=>e.preventDefault();
  for(const [id,value] of [['brush-upper',1],['brush-lower',0]])$(id).onclick=()=>{state.brush=value;$('brush-upper').classList.toggle('active',value===1);$('brush-lower').classList.toggle('active',value===0);};
  $('undo').onclick=()=>{const mixed=state.view==='mixed',history=mixed?state.mixedHistory:state.history;if(history.length){state[mixed?'mixedGrid':'grid']=history.pop();drawMap();}};
  for(const [id,view] of [['view-map','map'],['view-mixed','mixed'],['view-repeat','repeat']])$(id).onclick=()=>{state.view=view;for(const v of ['map','mixed','repeat'])$('view-'+v).classList.toggle('active',view===v);refresh();};
  $('show-grid').onchange=drawMap;$('zoom').onchange=drawMap;
  const resetMap=()=>{const mixed=state.view==='mixed',history=mixed?state.mixedHistory:state.history,key=mixed?'mixedGrid':'grid';history.push(state[key].map(r=>r.slice()));if(history.length>40)history.shift();state[key]=mixed?mixedScene():createScene($('scene').value);drawMap();};$('scene').onchange=resetMap;$('reset-map').onclick=resetMap;
  $('export-pack').onclick=guarded(exportPack);
  $('export-atlas').onclick=guarded(async()=>X.download(await X.png(atlas()),`tile-forge-${state.mode}-47.png`));
  $('export-tile').onclick=guarded(async()=>X.download(await X.png(tile(state.mask)),`tile-${state.mode}-${E.normalizeMask(state.mask)}.png`));
  $('import-override').onclick=()=>{if(!ready())return;$('override-file').click();};
  $('override-file').onchange=guarded(async()=>{
    const file=$('override-file').files[0];$('override-file').value='';if(!file)return;
    const key=`${state.mode}:${E.normalizeMask(state.mask)}`,size=state.size;
    if(file.size>1024*1024)throw new Error('Choose a single tile PNG up to 1 MB.');await requirePNG(file);
    const dataUrl=await readFile(file),image=await imageFromURL(dataUrl);
    if(image.width!==size||image.height!==size)throw new Error(`Replacement must be exactly ${size} × ${size} pixels.`);
    if(state.size!==size)throw new Error('Tile size changed while loading the replacement; import again.');
    state.overrides[key]={dataUrl,image};$('override-file').value='';refresh();status('Hand-drawn replacement applied. It is included in project and atlas exports.');
  });
  $('clear-override').onclick=()=>{delete state.overrides[`${state.mode}:${E.normalizeMask(state.mask)}`];refresh();status('Generated tile restored.');};
  $('save-project').onclick=guarded(async()=>{const blob=new Blob([JSON.stringify(project())],{type:'application/json'});if(blob.size>40*1024*1024)throw new Error('Project exceeds 40 MB. Use smaller source PNGs or fewer replacements.');await X.download(blob,'tile-forge-project.json');status('Project download ready, with embedded source PNGs and both painted scenes. Link below.');});
  $('load-project').onclick=()=>$('project-file').click();$('project-file').onchange=guarded(async()=>{const file=$('project-file').files[0];$('project-file').value='';if(file)await loadProject(file);});
  state.grid=createScene('cavern');setLimits();refresh();
  guarded(async()=>{
    if(location.protocol==='file:'){status('Use + PNG to load artwork, or launch the local server to load the Sil-More atlas.');return;}
    const src=await sourceFrom('source-atlas.png','Sil-More · 16x16.png','sil-more');state.sources.push(src);assignDefaults(src);defaultMixed();applyPreset('earth');status(`Loaded Sil-More atlas · ${src.image.width/state.size} × ${src.image.height/state.size} cells · original file is read-only.`);
  })();
})();
