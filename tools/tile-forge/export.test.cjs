'use strict';
const test=require('node:test');
const assert=require('node:assert/strict');
const {zip,crc32}=require('./export.js');

test('CRC matches the standard ZIP check vector',()=>{
  assert.equal(crc32(new TextEncoder().encode('123456789')),0xcbf43926);
});

test('ZIP stores UTF-8 names, binary content, valid checksums and directory offsets',async()=>{
  const binary=new Uint8Array([0,255,3,8,12,0]);
  const blob=await zip([{name:'tiles/mask-000.png',data:new Blob([binary])},{name:'notes-é.json',data:'{"mask":0}\n'}]);
  const data=new Uint8Array(await blob.arrayBuffer()),v=new DataView(data.buffer),decoder=new TextDecoder();
  const end=data.length-22;
  assert.equal(v.getUint32(end,true),0x06054b50);assert.equal(v.getUint16(end+10,true),2);
  const directoryStart=v.getUint32(end+16,true),directorySize=v.getUint32(end+12,true);
  assert.equal(directoryStart+directorySize,end);
  let cursor=directoryStart;
  for(const expected of [{name:'tiles/mask-000.png',bytes:binary},{name:'notes-é.json',bytes:new TextEncoder().encode('{"mask":0}\n')}]){
    assert.equal(v.getUint32(cursor,true),0x02014b50);assert.equal(v.getUint16(cursor+8,true),0x800);
    const nameLength=v.getUint16(cursor+28,true),offset=v.getUint32(cursor+42,true),length=v.getUint32(cursor+24,true);
    assert.equal(decoder.decode(data.slice(cursor+46,cursor+46+nameLength)),expected.name);
    assert.equal(v.getUint32(offset,true),0x04034b50);assert.equal(v.getUint16(offset+8,true),0);
    const localNameLength=v.getUint16(offset+26,true),payload=data.slice(offset+30+localNameLength,offset+30+localNameLength+length);
    assert.deepEqual(payload,expected.bytes);assert.equal(v.getUint32(offset+14,true),crc32(payload));assert.equal(v.getUint32(cursor+16,true),crc32(payload));
    cursor+=46+nameLength;
  }
  assert.equal(cursor,end);
});
