#!/usr/bin/env python3
"""自制合成素材，经真实C算法处理，输出WAV与客观指标；无第三方音乐。"""
import argparse,array,json,math,subprocess,wave
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--driver',required=True);p.add_argument('--output',required=True);a=p.parse_args();out=Path(a.output);out.mkdir(exist_ok=True,parents=True)
def metrics(pcm):
 l=pcm[0::2];r=pcm[1::2];n=len(l);lr=sum(x*y for x,y in zip(l,r));ll=sum(x*x for x in l);rr=sum(x*x for x in r)
 mono=[(x+y)/2 for x,y in zip(l,r)];side=[(x-y)/2 for x,y in zip(l,r)]
 return {'RMS':math.sqrt((ll+rr)/(n*2)), '峰值':max(map(abs,pcm)), '边界采样数':sum(x in (-32768,32767) for x in pcm),
 '声道相关性':lr/math.sqrt(ll*rr) if ll*rr else None, '单声道折叠RMS':math.sqrt(sum(x*x for x in mono)/n),'Side_RMS':math.sqrt(sum(x*x for x in side)/n)}
def wav(path,pcm,rate):
 with wave.open(str(path),'wb') as f:f.setnchannels(2);f.setsampwidth(2);f.setframerate(rate);f.writeframes(pcm.tobytes())
records=[]
for rate in (44100,48000):
 for kind in ('中置模拟人声','左右硬分离','中置加侧乐器','静音','满幅交替'):
  pcm=array.array('h')
  for i in range(rate*2):
   t=i/rate;c=6000*math.sin(2*math.pi*700*t)+3000*math.sin(2*math.pi*1400*t);l=5000*math.sin(2*math.pi*220*t);r=4000*math.sin(2*math.pi*330*t)
   x,y=(c,c) if kind=='中置模拟人声' else (l,r) if kind=='左右硬分离' else (c+l,c+r) if kind=='中置加侧乐器' else (0,0) if kind=='静音' else ((32767,-32768) if i%2 else (-32768,32767))
   pcm.extend((round(x),round(y)))
  before=metrics(pcm[rate*2:]);wav(out/f'{rate}-{kind}-输入.wav',pcm,rate)
  for mode in range(3):
   processed=subprocess.run([a.driver,str(rate),str(mode),'1','1'],input=pcm.tobytes(),stdout=subprocess.PIPE,check=True).stdout
   data=array.array('h');data.frombytes(processed);after=metrics(data[rate*2:]);wav(out/f'{rate}-{kind}-模式{mode}.wav',data,rate)
   row={'采样率':rate,'素材':kind,'模式':mode,'输入':before,'输出':after}
   if before['RMS'] and after['RMS']:row['RMS变化dB']=20*math.log10(after['RMS']/before['RMS'])
   records.append(row)
(out/'指标.json').write_text(json.dumps(records,ensure_ascii=False,indent=2)+'\n')
print(f'已验证真实C处理结果：{len(records)}组，WAV/指标保存于 {out}')
