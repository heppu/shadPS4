set pagination off
set confirm off
set print thread-events off
handle SIGSEGV SIGBUS SIGILL SIGUSR1 SIGUSR2 SIGPWR SIGXCPU nostop noprint pass
python
import gdb,json
rows=[]
class Capture(gdb.Breakpoint):
 def stop(self):
  r={}
  try:
   i=gdb.parse_and_eval('this->slot_images.values[image_id.index]')['info']
   r={'format':str(i['pixel_format']),'type':str(i['type']),'bits':int(i['num_bits']),'samples':int(i['num_samples']),'tiled':str(i['tile_mode']),'block':int(i['props']['is_block']),'width':int(i['size']['width']),'height':int(i['size']['height']),'depth':int(i['size']['depth']),'levels':int(i['resources']['levels']),'layers':int(i['resources']['layers']),'bytes':int(i['guest_size']),'frame':int(gdb.parse_and_eval('this->liverpool->flip_count'))}
  except Exception as e:r['error']=str(e)
  rows.append(r)
  if len(rows)>=8:
   open('/out/performance/image-download-formats.json','w').write(json.dumps(rows,indent=2));return True
  return False
Capture('VideoCore::TextureCache::DownloadImageMemory')
end
continue
detach
quit
