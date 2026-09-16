/* Dependency-free PNG packaging. ZIP entries use STORE (no compression). */
(function (root) {
  'use strict';
  const encoder = new TextEncoder();
  const table = Uint32Array.from({length:256}, (_, n) => {
    let c = n;
    for (let k=0;k<8;k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    return c >>> 0;
  });
  function crc32(bytes) {
    let crc=0xffffffff;
    for (const byte of bytes) crc=table[(crc ^ byte)&255] ^ (crc >>> 8);
    return (crc ^ 0xffffffff) >>> 0;
  }
  async function zip(files) {
    const parts=[], directory=[];
    let offset=0, directorySize=0;
    for (const file of files) {
      const name=encoder.encode(file.name);
      const data=typeof file.data === 'string' ? encoder.encode(file.data) : new Uint8Array(await file.data.arrayBuffer());
      const crc=crc32(data);
      const header=new Uint8Array(30+name.length), h=new DataView(header.buffer);
      h.setUint32(0,0x04034b50,true);h.setUint16(4,20,true);h.setUint16(6,0x800,true);
      h.setUint16(12,33,true);h.setUint32(14,crc,true);h.setUint32(18,data.length,true);h.setUint32(22,data.length,true);h.setUint16(26,name.length,true);header.set(name,30);
      const central=new Uint8Array(46+name.length), c=new DataView(central.buffer);
      c.setUint32(0,0x02014b50,true);c.setUint16(4,20,true);c.setUint16(6,20,true);c.setUint16(8,0x800,true);
      c.setUint16(14,33,true);c.setUint32(16,crc,true);c.setUint32(20,data.length,true);c.setUint32(24,data.length,true);c.setUint16(28,name.length,true);c.setUint32(42,offset,true);central.set(name,46);
      parts.push(header,data);directory.push(central);offset+=header.length+data.length;directorySize+=central.length;
    }
    const end=new Uint8Array(22), e=new DataView(end.buffer);
    e.setUint32(0,0x06054b50,true);e.setUint16(8,files.length,true);e.setUint16(10,files.length,true);e.setUint32(12,directorySize,true);e.setUint32(16,offset,true);
    return new Blob([...parts,...directory,end],{type:'application/zip'});
  }
  async function download(blob,name) {
    let url,savedPath;
    if(location.protocol==='http:'&&location.hostname==='127.0.0.1'){
      try{
        const response=await fetch('/export',{method:'POST',headers:{'Content-Type':blob.type,'X-Tile-Forge-Name':name},body:blob});
        if(!response.ok)throw new Error('Local export unavailable');
        const saved=await response.json();url=saved.url;savedPath=saved.path;
      }catch(error){console.warn('Using browser download fallback:',error.message);}
    }
    if(!url)url=URL.createObjectURL(blob);
    const a=document.createElement('a');
    a.href=url;a.download=name;a.textContent='Download '+name;a.className='download-link';
    if(savedPath)a.title='Also saved locally: '+savedPath;
    const shelf=document.getElementById('download-shelf');
    if(shelf){
      shelf.prepend(a);
      while(shelf.children.length>4){const old=shelf.lastChild;URL.revokeObjectURL(old.href);old.remove();}
      shelf.hidden=false;a.click();
    }else{document.body.append(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),30000);}
  }
  function png(canvas) { return new Promise((resolve,reject)=>canvas.toBlob(b=>b?resolve(b):reject(new Error('PNG export failed. Load the source PNG through + PNG.')),'image/png')); }
  const api={zip,crc32,download,png};
  if (typeof module === 'object' && module.exports) module.exports=api;
  else root.ForgeExport=api;
})(typeof globalThis !== 'undefined' ? globalThis : this);
