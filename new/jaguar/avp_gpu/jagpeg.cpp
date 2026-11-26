#include "jaguar/avp_gpu/jagpeg.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>

namespace avp {
namespace {
uint16_t be16(const uint8_t* p){ return uint16_t((uint16_t(p[0])<<8)|p[1]); }

struct Bits {
  const uint8_t* p{}; size_t n{}; size_t bit{};
  int get(){ if(bit>=n*8) throw std::runtime_error("JagPEG entropy underrun"); int v=(p[bit>>3]>>(7-(bit&7)))&1; ++bit; return v; }
  uint32_t getn(int k){ uint32_t v=0; while(k--) v=(v<<1)|uint32_t(get()); return v; }
};

struct Huff {
  std::array<int,17> mincode{}, maxcode{}, valptr{};
  std::vector<uint8_t> vals;
  Huff(const std::array<uint8_t,16>& counts, const std::vector<uint8_t>& v):vals(v){
    int code=0,k=0;
    mincode.fill(-1); maxcode.fill(-1); valptr.fill(0);
    for(int len=1;len<=16;++len){
      const int c=counts[size_t(len-1)];
      if(c){ mincode[len]=code; valptr[len]=k; code+=c-1; maxcode[len]=code; ++code; k+=c; }
      code <<= 1;
    }
  }
  uint8_t decode(Bits& b) const {
    int code=0;
    for(int len=1;len<=16;++len){
      code=(code<<1)|b.get();
      if(maxcode[len]>=0 && code<=maxcode[len]) return vals[size_t(valptr[len]+code-mincode[len])];
    }
    throw std::runtime_error("invalid JagPEG Huffman code");
  }
};

const Huff& dc_huff(){
  static const std::array<uint8_t,16> c{{0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0}};
  static const Huff h(c,{0,1,2,3,4,5,6,7,8,9,10,11}); return h;
}
const Huff& ac_huff(){
  static const std::array<uint8_t,16> c{{0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,0x7d}};
  static const std::vector<uint8_t> v={
    0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,
    0x07,0x22,0x71,0x14,0x32,0x81,0x91,0xa1,0x08,0x23,0x42,0xb1,0xc1,0x15,0x52,
    0xd1,0xf0,0x24,0x33,0x62,0x72,0x82,0x09,0x0a,0x16,0x17,0x18,0x19,0x1a,0x25,
    0x26,0x27,0x28,0x29,0x2a,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,
    0x46,0x47,0x48,0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,
    0x65,0x66,0x67,0x68,0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x83,
    0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,
    0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,
    0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,
    0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,
    0xe9,0xea,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa};
  static const Huff h(c,v); return h;
}

int extend(uint32_t v,int bits){ if(bits==0) return 0; const uint32_t vt=1u<<(bits-1); if(v<vt) return int(v)-int((1u<<bits)-1u); return int(v); }

constexpr std::array<int,64> zig{{
  0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,
  12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
  35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,
  58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63}};
constexpr std::array<int,64> qbase{{
 16,11,10,16,24,40,51,61,
 12,12,14,19,26,58,60,55,
 14,13,16,24,40,57,69,56,
 14,17,22,29,51,87,80,62,
 18,22,37,56,68,109,103,77,
 24,35,55,64,81,104,113,92,
 49,64,78,87,103,121,120,101,
 72,92,95,98,112,100,103,99}};

std::array<int,64> quant(int quality){
  quality=std::clamp(quality,1,100); int scale=quality<50?5000/quality:200-2*quality;
  std::array<int,64> q{}; for(size_t i=0;i<64;++i) q[i]=std::clamp((qbase[i]*scale+50)/100,1,255); return q;
}

std::int32_t wrap_add(std::int32_t a,std::int32_t b){ return std::int32_t(std::uint32_t(a)+std::uint32_t(b)); }
std::int32_t wrap_sub(std::int32_t a,std::int32_t b){ return std::int32_t(std::uint32_t(a)-std::uint32_t(b)); }
std::int32_t wrap_shl(std::int32_t a,unsigned n){ return std::int32_t(std::uint32_t(a)<<n); }
std::int32_t asr(std::int32_t v,unsigned n){
  if(!n) return v;
  const std::uint32_t u=std::uint32_t(v);
  if((u&0x80000000u)==0) return std::int32_t(u>>n);
  return std::int32_t((u>>n) | (~std::uint32_t(0)<<(32-n)));
}
std::int32_t imult16(std::int32_t a,std::int32_t b){
  return std::int32_t(std::int16_t(a&0xffff) * std::int32_t(std::int16_t(b&0xffff)));
}

// Literal scalar translation of the 8-point transform used by AVP's dejag16 GPU
// program. Register arithmetic intentionally wraps to 32 bits, IMULT consumes the
// signed low 16 bits, and the two passes use the retail rounding shifts.
void jag_idct_1d(const std::int32_t* in,std::int32_t* out,unsigned shift,std::int32_t round){
  constexpr std::int32_t c8  =  0x098e;
  constexpr std::int32_t c9  = -0x0c7c;
  constexpr std::int32_t c10 =  0x1151;
  constexpr std::int32_t c11 =  0x187e;
  constexpr std::int32_t c12 = -0x1ccd;
  constexpr std::int32_t c13 =  0x25a1;
  constexpr std::int32_t c6  =  0x300b;
  constexpr std::int32_t c7  = -0x3b21;
  constexpr std::int32_t c26 = -0x3ec5;
  constexpr std::int32_t c27 =  0x41b3;
  constexpr std::int32_t c28 = -0x5203;
  constexpr std::int32_t c29 =  0x6254;

  std::int32_t r2=in[2], r3=in[6];
  std::int32_t r1=wrap_add(r2,r3);
  std::int32_t r18=wrap_add(imult16(c7,r3),imult16(c10,r1));
  std::int32_t r19=wrap_add(imult16(c11,r2),imult16(c10,r1));

  std::int32_t r16=wrap_shl(wrap_add(in[0],in[4]),13);
  std::int32_t r17=wrap_shl(wrap_sub(in[0],in[4]),13);
  std::int32_t r20=wrap_add(r16,r19), r23=wrap_sub(r16,r19);
  std::int32_t r21=wrap_add(r17,r18), r22=wrap_sub(r17,r18);

  r19=in[1]; r18=in[3]; r17=in[5]; r16=in[7];
  r1=imult16(c12,wrap_add(r16,r19));
  r2=imult16(c28,wrap_add(r17,r18));
  r3=wrap_add(r16,r18);
  std::int32_t r4=wrap_add(r17,r19);
  std::int32_t r5=imult16(c13,wrap_add(r3,r4));
  r16=imult16(c8,r16);
  r17=imult16(c27,r17);
  r18=imult16(c29,r18);
  r19=imult16(c6,r19);
  r3=wrap_add(imult16(c26,r3),r5);
  r4=wrap_add(imult16(c9,r4),r5);
  r16=wrap_add(wrap_add(r16,r3),r1);
  r17=wrap_add(wrap_add(r17,r4),r2);
  r19=wrap_add(wrap_add(r19,r1),r4);
  r18=wrap_add(wrap_add(r18,r2),r3);

  auto q=[&](std::int32_t v){ return asr(wrap_add(v,round),shift); };
  out[0]=q(wrap_add(r20,r19));
  out[1]=q(wrap_add(r21,r18));
  out[2]=q(wrap_add(r22,r17));
  out[3]=q(wrap_add(r23,r16));
  out[4]=q(wrap_sub(r23,r16));
  out[5]=q(wrap_sub(r22,r17));
  out[6]=q(wrap_sub(r21,r18));
  out[7]=q(wrap_sub(r20,r19));
}

void exact_idct(const std::array<int,64>& coef,std::array<std::int32_t,64>& out){
  std::array<std::int32_t,64> tmp{};
  for(int y=0;y<8;++y){
    std::int32_t row[8],dst[8];
    for(int x=0;x<8;++x) row[x]=coef[size_t(y*8+x)];
    jag_idct_1d(row,dst,11,0x400);
    for(int x=0;x<8;++x) tmp[size_t(y*8+x)]=dst[x];
  }
  for(int x=0;x<8;++x){
    std::int32_t col[8],dst[8];
    for(int y=0;y<8;++y) col[y]=tmp[size_t(y*8+x)];
    jag_idct_1d(col,dst,18,0x20000);
    for(int y=0;y<8;++y) out[size_t(y*8+x)]=dst[y];
  }
}

void decode_block(Bits& bits,int& pred,const std::array<int,64>& q,std::array<std::int32_t,64>& out){
  std::array<int,64> coef{};
  int s=dc_huff().decode(bits); pred += extend(bits.getn(s),s); coef[0]=pred*q[0];
  int k=1;
  while(k<64){ uint8_t rs=ac_huff().decode(bits); if(rs==0) break; int r=rs>>4, n=rs&15; if(rs==0xf0){k+=16;continue;} k+=r; if(k>=64) throw std::runtime_error("JagPEG AC run overflow"); int val=extend(bits.getn(n),n); int pos=zig[size_t(k)]; coef[pos]=val*q[size_t(pos)]; ++k; }
  exact_idct(coef,out);
}

uint8_t sat_i32(std::int32_t v){ return uint8_t(std::clamp<std::int32_t>(v,0,255)); }

}

RgbImage decode_jagpeg(const std::vector<uint8_t>& bytes,int quality,bool swap_chroma){
  if(bytes.size()<8) throw std::runtime_error("JagPEG file too small");
  const int version=be16(bytes.data()), factor=be16(bytes.data()+2), bw=be16(bytes.data()+4), bh=be16(bytes.data()+6);
  if(version!=1 || (factor!=1 && factor!=2) || bw<=0 || bh<=0) throw std::runtime_error("unsupported JagPEG header");
  const int width=bw*8, height=bh*8;
  RgbImage img{width,height,std::vector<uint8_t>(size_t(width)*size_t(height)*3)};
  Bits bits{bytes.data()+8,bytes.size()-8,0}; auto q=quant(quality); int predY=0,predCb=0,predCr=0;
  std::array<std::int32_t,64> y0{},y1{},cb{},cr{};
  if(factor==2){
    // Encoded units are 16x8 output pixels: Y0,Cb,Cr,Y1. Cb/Cr are horizontally upsampled.
    for(int by=0;by<bh;++by){
      for(int bx=0;bx<bw;bx+=2){
        decode_block(bits,predY,q,y0); decode_block(bits,predCb,q,cb); decode_block(bits,predCr,q,cr); decode_block(bits,predY,q,y1);
        for(int yy=0;yy<8;++yy) for(int xx=0;xx<16;++xx){
          const int px=bx*8+xx, py=by*8+yy; if(px>=width) continue;
          const std::int32_t Y=(xx<8?y0:y1)[size_t(yy*8+(xx&7))]+128;
          const int cx=std::min(7,xx>>1);
          const std::int32_t Cb=(swap_chroma?cr:cb)[size_t(yy*8+cx)], Cr=(swap_chroma?cb:cr)[size_t(yy*8+cx)];
          auto* d=&img.rgb[(size_t(py)*size_t(width)+size_t(px))*3];
          d[0]=sat_i32(Y + asr(imult16(Cr,0x59ba),14));
          d[2]=sat_i32(Y + asr(imult16(Cb,0x7168),14));
          d[1]=sat_i32(Y + asr(wrap_add(imult16(Cb,-0x1606),imult16(Cr,-0x2db4)),14));
        }
      }
    }
  } else {
    for(int by=0;by<bh;++by) for(int bx=0;bx<bw;++bx){
      decode_block(bits,predY,q,y0); decode_block(bits,predCb,q,cb); decode_block(bits,predCr,q,cr);
      for(int yy=0;yy<8;++yy) for(int xx=0;xx<8;++xx){
        const int px=bx*8+xx,py=by*8+yy; const size_t i=size_t(yy*8+xx);
        const std::int32_t Y=y0[i]+128, Cb=(swap_chroma?cr:cb)[i], Cr=(swap_chroma?cb:cr)[i];
        auto* d=&img.rgb[(size_t(py)*width+px)*3];
        d[0]=sat_i32(Y + asr(imult16(Cr,0x59ba),14));
        d[2]=sat_i32(Y + asr(imult16(Cb,0x7168),14));
        d[1]=sat_i32(Y + asr(wrap_add(imult16(Cb,-0x1606),imult16(Cr,-0x2db4)),14));
      }
    }
  }
  return img;
}

void write_ppm(const std::filesystem::path& path,const RgbImage& image){ std::filesystem::create_directories(path.parent_path()); std::ofstream f(path,std::ios::binary); if(!f) throw std::runtime_error("cannot write PPM"); f<<"P6\n"<<image.width<<" "<<image.height<<"\n255\n"; f.write(reinterpret_cast<const char*>(image.rgb.data()),std::streamsize(image.rgb.size())); }
RgbImage read_ppm(const std::filesystem::path& path){ std::ifstream f(path,std::ios::binary); if(!f) throw std::runtime_error("cannot read PPM"); std::string magic; int w,h,maxv; f>>magic>>w>>h>>maxv; f.get(); if(magic!="P6"||w<=0||h<=0||maxv!=255) throw std::runtime_error("bad PPM"); RgbImage im{w,h,std::vector<uint8_t>(size_t(w)*h*3)}; f.read(reinterpret_cast<char*>(im.rgb.data()),std::streamsize(im.rgb.size())); if(!f) throw std::runtime_error("truncated PPM"); return im; }
} // namespace avp
