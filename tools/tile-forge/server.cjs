#!/usr/bin/env node
'use strict';
const http=require('node:http');
const fs=require('node:fs');
const path=require('node:path');
const {spawn}=require('node:child_process');
const root=__dirname;
const output=path.resolve(root,'../../scripts/output/tile-forge');
const exportName=/^[a-zA-Z0-9][a-zA-Z0-9._-]{0,120}\.(png|json|zip)$/;
const written=new Set();
const routes={
  '/':'index.html','/index.html':'index.html','/style.css':'style.css',
  '/engine.js':'engine.js','/materials.js':'materials.js','/app.js':'app.js','/export.js':'export.js','/README.md':'README.md',
  '/source-atlas.png':path.resolve(root,'../../lib/xtra/graf/16x16.png')
};
const types={'.html':'text/html; charset=utf-8','.css':'text/css; charset=utf-8','.js':'text/javascript; charset=utf-8','.md':'text/plain; charset=utf-8','.png':'image/png'};
const portArg=process.argv.find(arg=>arg.startsWith('--port='));
const port=portArg?Number(portArg.slice(7)):8787;
if (!Number.isInteger(port)||port<0||port>65535) throw new Error('Invalid port. Use --port=8787.');
const server=http.createServer((req,res)=>{
  const pathname=new URL(req.url,'http://127.0.0.1').pathname;
  if(pathname==='/export'&&req.method==='POST'){
    const name=req.headers['x-tile-forge-name'];
    const origin=req.headers.origin;
    if(!name||!exportName.test(name)||origin!==`http://127.0.0.1:${server.address().port}`){res.writeHead(403);res.end('Local export only');return;}
    const chunks=[];let length=0,tooLarge=false;
    req.on('data',chunk=>{length+=chunk.length;if(length>50*1024*1024){tooLarge=true;chunks.length=0;}else if(!tooLarge)chunks.push(chunk);});
    req.on('end',async()=>{
      if(tooLarge){res.writeHead(413);res.end('Export exceeds 50 MB');return;}
      try{
        await fs.promises.mkdir(output,{recursive:true});
        const unique=`${Date.now()}-${require('node:crypto').randomBytes(4).toString('hex')}-${name}`;
        await fs.promises.writeFile(path.join(output,unique),Buffer.concat(chunks),{flag:'wx'});written.add(unique);
        res.writeHead(201,{'Content-Type':'application/json','Cache-Control':'no-store'});res.end(JSON.stringify({url:`/exports/${unique}`,path:path.join(output,unique)}));
      }catch(error){res.writeHead(500);res.end('Could not save export: '+error.message);}
    });
    return;
  }
  if(pathname.startsWith('/exports/')&&['GET','HEAD'].includes(req.method)){
    const name=pathname.slice('/exports/'.length);
    if(!written.has(name)){res.writeHead(404);res.end('Export unavailable. Generate it again.');return;}
    const file=path.join(output,name);
    fs.readFile(file,(error,data)=>{if(error){res.writeHead(404);res.end();return;}res.writeHead(200,{'Content-Type':name.endsWith('.zip')?'application/zip':name.endsWith('.json')?'application/json':'image/png','Content-Disposition':`attachment; filename="${name}"`,'X-Content-Type-Options':'nosniff'});res.end(req.method==='HEAD'?undefined:data);});
    return;
  }
  if (!['GET','HEAD'].includes(req.method)) {res.writeHead(405);res.end();return;}
  if (!Object.hasOwn(routes,pathname)) {res.writeHead(404);res.end('Not found');return;}
  const file=path.resolve(root,routes[pathname]);
  fs.readFile(file,(error,data)=>{
    if(error){res.writeHead(404);res.end('File unavailable');return;}
    res.writeHead(200,{'Content-Type':types[path.extname(file)]||'application/octet-stream','Cache-Control':'no-store','X-Content-Type-Options':'nosniff'});
    res.end(req.method==='HEAD'?undefined:data);
  });
});
server.on('error',error=>{console.error(error.code==='EADDRINUSE'?`Port ${port} is occupied. Try node server.cjs --port=${port+1} --open`:error.message);process.exitCode=1;});
server.listen(port,'127.0.0.1',()=>{
  const url=`http://127.0.0.1:${server.address().port}/`;
  console.log(`Tile Forge: ${url}\nRead-only source atlas. Exports saved in ${output}. Ctrl+C to stop.`);
  if(process.argv.includes('--open')) {
    const command=process.platform==='win32'?'explorer.exe':process.platform==='darwin'?'open':'xdg-open';
    const child=spawn(command,[url],{detached:true,stdio:'ignore',windowsHide:true});
    child.on('error',()=>console.log(`Open ${url} in your browser.`));child.unref();
  }
});
