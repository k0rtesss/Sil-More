'use strict';
const test=require('node:test');
const assert=require('node:assert/strict');
const http=require('node:http');
const path=require('node:path');
const {spawn}=require('node:child_process');

function request(port,target,method='GET',headers={}) {
  return new Promise((resolve,reject)=>{
    const req=http.request({host:'127.0.0.1',port,path:target,method,headers},res=>{
      const chunks=[];
      res.on('data',chunk=>chunks.push(chunk));
      res.on('end',()=>resolve({status:res.statusCode,headers:res.headers,body:Buffer.concat(chunks).toString()}));
    });
    req.setTimeout(3000,()=>req.destroy(new Error('Request timed out')));
    req.on('error',reject);req.end();
  });
}

test('malformed request targets are rejected without stopping the local server',{timeout:10000},async t=>{
  const child=spawn(process.execPath,[path.join(__dirname,'server.cjs'),'--port=0'],
    {stdio:['ignore','pipe','pipe'],windowsHide:true});
  t.after(()=>child.kill());
  const port=await new Promise((resolve,reject)=>{
    let output='';
    child.stdout.on('data',data=>{
      output+=data;
      const match=output.match(/Tile Forge: http:\/\/127\.0\.0\.1:(\d+)\//);
      if(match)resolve(Number(match[1]));
    });
    child.on('error',reject);
    child.on('exit',code=>reject(new Error('Server exited: '+code)));
  });
  assert.equal((await request(port,'/')).status,200);
  assert.equal((await request(port,'//[')).status,400);
  const healthy=await request(port,'/engine.js','HEAD');
  assert.equal(healthy.status,200);assert.equal(healthy.body,'');
  assert.equal(healthy.headers['x-content-type-options'],'nosniff');
  assert.equal((await request(port,'/export','POST',{'x-tile-forge-name':'check.json'})).status,403);
  assert.equal((await request(port,'/../server.cjs')).status,404);
});
