//------------------------------------------------------------------------------
//  Shader code for loadpng-sapp sample.
//------------------------------------------------------------------------------
@vs vs
uniform vs_params {
    mat4 mvp;
};

in vec3 pos;
in vec2 texcoord0;
in vec4 color0;

out vec2 vTexCoord;
out vec4 color;

void main() {
    gl_Position = mvp * vec4(pos, 1.0f);
    vTexCoord = texcoord0;
    color = color0;
}
@end

@fs fs

#define CRT_GAMMA 2.4
#define SCANLINE_THINNESS 0.5
#define SCAN_BLUR 2.5
#define MASK_INTENSITY 0.54
#define CURVATURE 0.02
#define CORNER 3.0
#define MASK 1.0
#define TRINITRON_CURVE 0.0

uniform sampler2D Source;

in vec2 vTexCoord;
in vec4 color;

uniform crt_params {
    vec4 SourceSize;
    vec4 OutputSize;
};

out vec4 FragColor;

float FromSrgb1(float c){
    return (c<=0.04045)?c*(1.0/12.92):
        pow(abs(c)*(1.0/1.055)+(0.055/1.055),CRT_GAMMA);}

vec3 FromSrgb(vec3 c){return vec3(
    FromSrgb1(c.r),FromSrgb1(c.g),FromSrgb1(c.b));}

float ToSrgb1(float c){
    return(c<0.0031308?c*12.92:1.055*pow(c,0.41666)-0.055);}

vec3 ToSrgb(vec3 c){return vec3(
    ToSrgb1(c.r),ToSrgb1(c.g),ToSrgb1(c.b));}

#define saturate(x) clamp((x),0.0,1.0)

//==============================================================
//                      SETUP FOR CRTS
//--------------------------------------------------------------
//==============================================================
//#define CRTS_DEBUG 1
#define CRTS_GPU 1
#define CRTS_GLSL 1
//--------------------------------------------------------------
//#define CRTS_2_TAP 1
//--------------------------------------------------------------
#define CRTS_TONE 1
#define CRTS_CONTRAST 0
#define CRTS_SATURATION 0
//--------------------------------------------------------------
#define CRTS_WARP 1
//--------------------------------------------------------------
// Try different masks -> moved to runtime parameters
//#define CRTS_MASK_GRILLE 1
//#define CRTS_MASK_GRILLE_LITE 1
//#define CRTS_MASK_NONE 1
//#define CRTS_MASK_SHADOW 1
//--------------------------------------------------------------
// Scanline thinness
//  0.50 = fused scanlines
//  0.70 = recommended default
//  1.00 = thinner scanlines (too thin)
#define INPUT_THIN (0.5 + (0.5 * SCANLINE_THINNESS))
//--------------------------------------------------------------
// Horizonal scan blur
//  -3.0 = pixely
//  -2.5 = default
//  -2.0 = smooth
//  -1.0 = too blurry
#define INPUT_BLUR (-1.0 * SCAN_BLUR)
//--------------------------------------------------------------
// Shadow mask effect, ranges from,
//  0.25 = large amount of mask (not recommended, too dark)
//  0.50 = recommended default
//  1.00 = no shadow mask
#define INPUT_MASK (1.0 - MASK_INTENSITY)
//--------------------------------------------------------------
#define INPUT_X SourceSize.x
#define INPUT_Y SourceSize.y
//--------------------------------------------------------------
// Setup the function which returns input image color
vec3 CrtsFetch(vec2 uv){
 // For shadertoy, scale to get native texels in the image
 uv*=vec2(INPUT_X,INPUT_Y)/SourceSize.xy;
 // Move towards intersting parts
// uv+=vec2(0.5,0.5);
 // Non-shadertoy case would not have the color conversion
 return FromSrgb(texture(Source,uv.xy,-16.0).rgb);}

#define saturate(x) clamp((x),0.0,1.0)

float max3f(float a,float b,float c){
   return max(a,max(b,c));}

//_____________________________/\_______________________________
//==============================================================
//              TONAL CONTROL CONSTANT GENERATION
//--------------------------------------------------------------
// This is in here for rapid prototyping
// Please use the CPU code and pass in as constants
//==============================================================
 vec4 CrtsTone(
 float contrast,
 float saturation,
 float thin,
 float mask){
//--------------------------------------------------------------
  if(MASK == 0.0) mask=1.0;
//--------------------------------------------------------------
  if(MASK == 1.0){
   // Normal R mask is {1.0,mask,mask}
   // LITE   R mask is {mask,1.0,1.0}
   mask=0.5+mask*0.5;
   }
//--------------------------------------------------------------
  vec4 ret;
  float midOut=0.18/((1.5-thin)*(0.5*mask+0.5));
  float pMidIn=pow(0.18,contrast);
  ret.x=contrast;
  ret.y=((-pMidIn)+midOut)/((1.0-pMidIn)*midOut);
  ret.z=((-pMidIn)*midOut+pMidIn)/(midOut*(-pMidIn)+midOut);
  ret.w=contrast+saturation;
  return ret;}
//_____________________________/\_______________________________
//==============================================================
//                            MASK
//--------------------------------------------------------------
// Letting LCD/OLED pixel elements function like CRT phosphors
// So "phosphor" resolution scales with display resolution
//--------------------------------------------------------------
// Not applying any warp to the mask (want high frequency)
// Real aperture grille has a mask which gets wider on ends
// Not attempting to be "real" but instead look the best
//--------------------------------------------------------------
// Shadow mask is stretched horizontally
//  RRGGBB
//  GBBRRG
//  RRGGBB
// This tends to look better on LCDs than vertical
// Also 2 pixel width is required to get triad centered
//--------------------------------------------------------------
// The LITE version of the Aperture Grille is brighter
// Uses {dark,1.0,1.0} for R channel
// Non LITE version uses {1.0,dark,dark}
//--------------------------------------------------------------
// 'pos' - This is 'fragCoord.xy'
//         Pixel {0,0} should be {0.5,0.5}
//         Pixel {1,1} should be {1.5,1.5} 
//--------------------------------------------------------------
// 'dark' - Exposure of of masked channel
//          0.0=fully off, 1.0=no effect
//==============================================================
 vec3 CrtsMask(vec2 pos,float dark){
  if(MASK == 2.0){
   vec3 m=vec3(dark,dark,dark);
   float x=fract(pos.x*(1.0/3.0));
   if(x<(1.0/3.0))m.r=1.0;
   else if(x<(2.0/3.0))m.g=1.0;
   else m.b=1.0;
   return m;
  }
//--------------------------------------------------------------
  else if(MASK == 1.0){
   vec3 m=vec3(1.0,1.0,1.0);
   float x=fract(pos.x*(1.0/3.0));
   if(x<(1.0/3.0))m.r=dark;
   else if(x<(2.0/3.0))m.g=dark;
   else m.b=dark;
   return m;
  }
//--------------------------------------------------------------
  else if(MASK == 3.0){
   pos.x+=pos.y*2.9999;
   vec3 m=vec3(dark,dark,dark);
   float x=fract(pos.x*(1.0/6.0));
   if(x<(1.0/3.0))m.r=1.0;
   else if(x<(2.0/3.0))m.g=1.0;
   else m.b=1.0;
   return m;
  }
//--------------------------------------------------------------
  else{
   return vec3(1.0,1.0,1.0);
  }
 }
//_____________________________/\_______________________________
//==============================================================
//                        FILTER ENTRY
//--------------------------------------------------------------
// Input must be linear
// Output color is linear
//--------------------------------------------------------------
// Must have fetch function setup: vec3 CrtsFetch(vec2 uv)
//  - The 'uv' range is {0.0 to 1.0} for input texture
//  - Output of this must be linear color
//--------------------------------------------------------------
// SCANLINE MATH & AUTO-EXPOSURE NOTES
// ===================================
// Each output line has contribution from at most 2 scanlines
// Scanlines are shaped by a windowed cosine function
// This shape blends together well with only 2 lines of overlap
//--------------------------------------------------------------
// Base scanline intensity is as follows
// which leaves output intensity range from {0 to 1.0}
// --------
// thin := range {thick 0.5 to thin 1.0}
// off  := range {0.0 to <1.0}, 
//         sub-pixel offset between two scanlines
//  --------
//  a0=cos(min(0.5,     off *thin)*2pi)*0.5+0.5;
//  a1=cos(min(0.5,(1.0-off)*thin)*2pi)*0.5+0.5;
//--------------------------------------------------------------
// This leads to a image darkening factor of roughly: 
//  {(1.5-thin)/1.0}
// This is further reduced by the mask: 
//  {1.0/2.0+mask*1.0/2.0}
// Reciprocal of combined effect is used for auto-exposure
//  to scale up the mid-level in the tonemapper
//==============================================================
 vec3 CrtsFilter(
//--------------------------------------------------------------
  // SV_POSITION, fragCoord.xy
  vec2 ipos,
//--------------------------------------------------------------
  // inputSize / outputSize (in pixels)
  vec2 inputSizeDivOutputSize,     
//--------------------------------------------------------------
  // 0.5 * inputSize (in pixels)
  vec2 halfInputSize,
//--------------------------------------------------------------
  // 1.0 / inputSize (in pixels)
  vec2 rcpInputSize,
//--------------------------------------------------------------
  // 1.0 / outputSize (in pixels)
  vec2 rcpOutputSize,
//--------------------------------------------------------------
  // 2.0 / outputSize (in pixels)
  vec2 twoDivOutputSize,   
//--------------------------------------------------------------
  // inputSize.y
  float inputHeight,
//--------------------------------------------------------------
  // Warp scanlines but not phosphor mask
  //  0.0 = no warp
  //  1.0/64.0 = light warping
  //  1.0/32.0 = more warping
  // Want x and y warping to be different (based on aspect)
  vec2 warp,
//--------------------------------------------------------------
  // Scanline thinness
  //  0.50 = fused scanlines
  //  0.70 = recommended default
  //  1.00 = thinner scanlines (too thin)
  // Shared with CrtsTone() function
  float thin,
//--------------------------------------------------------------
  // Horizonal scan blur
  //  -3.0 = pixely
  //  -2.5 = default
  //  -2.0 = smooth
  //  -1.0 = too blurry
  float blur,
//--------------------------------------------------------------
  // Shadow mask effect, ranges from,
  //  0.25 = large amount of mask (not recommended, too dark)
  //  0.50 = recommended default
  //  1.00 = no shadow mask
  // Shared with CrtsTone() function
  float mask,
//--------------------------------------------------------------
  // Tonal curve parameters generated by CrtsTone()
  vec4 tone
//--------------------------------------------------------------
 ){
//--------------------------------------------------------------
//--------------------------------------------------------------
  // Optional apply warp
  vec2 pos;
  #ifdef CRTS_WARP
   // Convert to {-1 to 1} range
   pos=ipos*twoDivOutputSize-vec2(1.0,1.0);
   // Distort pushes image outside {-1 to 1} range
   pos*=vec2(
    1.0+(pos.y*pos.y)*warp.x,
    1.0+(pos.x*pos.x)*warp.y);
   // TODO: Vignette needs optimization
   float vin=(1.0-(
    (1.0-saturate(pos.x*pos.x))*(1.0-saturate(pos.y*pos.y)))) * (0.998 + (0.001 * CORNER));
   vin=saturate((-vin)*inputHeight+inputHeight);
   // Leave in {0 to inputSize}
   pos=pos*halfInputSize+halfInputSize;     
  #else
   pos=ipos*inputSizeDivOutputSize;
  #endif
//--------------------------------------------------------------
  // Snap to center of first scanline
  float y0=floor(pos.y-0.5)+0.5;
  #ifdef CRTS_2_TAP
   // Using Inigo's "Improved Texture Interpolation"
   // http://iquilezles.org/www/articles/texture/texture.htm
   pos.x+=0.5;
   float xi=floor(pos.x);
   float xf=pos.x-xi;
   xf=xf*xf*xf*(xf*(xf*6.0-15.0)+10.0);  
   float x0=xi+xf-0.5;
   vec2 p=vec2(x0*rcpInputSize.x,y0*rcpInputSize.y);     
   // Coordinate adjusted bilinear fetch from 2 nearest scanlines
   vec3 colA=CrtsFetch(p);
   p.y+=rcpInputSize.y;
   vec3 colB=CrtsFetch(p);
  #else
   // Snap to center of one of four pixels
   float x0=floor(pos.x-1.5)+0.5;
   // Inital UV position
   vec2 p=vec2(x0*rcpInputSize.x,y0*rcpInputSize.y);     
   // Fetch 4 nearest texels from 2 nearest scanlines
   vec3 colA0=CrtsFetch(p);
   p.x+=rcpInputSize.x;
   vec3 colA1=CrtsFetch(p);
   p.x+=rcpInputSize.x;
   vec3 colA2=CrtsFetch(p);
   p.x+=rcpInputSize.x;
   vec3 colA3=CrtsFetch(p);
   p.y+=rcpInputSize.y;
   vec3 colB3=CrtsFetch(p);
   p.x-=rcpInputSize.x;
   vec3 colB2=CrtsFetch(p);
   p.x-=rcpInputSize.x;
   vec3 colB1=CrtsFetch(p);
   p.x-=rcpInputSize.x;
   vec3 colB0=CrtsFetch(p);
  #endif
//--------------------------------------------------------------
  // Vertical filter
  // Scanline intensity is using sine wave
  // Easy filter window and integral used later in exposure
  float off=pos.y-y0;
  float pi2=6.28318530717958;
  float hlf=0.5;
  float scanA=cos(min(0.5,  off *thin     )*pi2)*hlf+hlf;
  float scanB=cos(min(0.5,(-off)*thin+thin)*pi2)*hlf+hlf;
//--------------------------------------------------------------
  #ifdef CRTS_2_TAP
   #ifdef CRTS_WARP
    // Get rid of wrong pixels on edge
    scanA*=vin;
    scanB*=vin;
   #endif
   // Apply vertical filter
   vec3 color=(colA*scanA)+(colB*scanB);
  #else
   // Horizontal kernel is simple gaussian filter
   float off0=pos.x-x0;
   float off1=off0-1.0;
   float off2=off0-2.0;
   float off3=off0-3.0;
   float pix0=exp2(blur*off0*off0);
   float pix1=exp2(blur*off1*off1);
   float pix2=exp2(blur*off2*off2);
   float pix3=exp2(blur*off3*off3);
   float pixT=(1.0/(pix0+pix1+pix2+pix3));
   #ifdef CRTS_WARP
    // Get rid of wrong pixels on edge
    pixT*=vin;
   #endif
   scanA*=pixT;
   scanB*=pixT;
   // Apply horizontal and vertical filters
   vec3 color=
    (colA0*pix0+colA1*pix1+colA2*pix2+colA3*pix3)*scanA +
    (colB0*pix0+colB1*pix1+colB2*pix2+colB3*pix3)*scanB;
  #endif
//--------------------------------------------------------------
  // Apply phosphor mask          
  color*=CrtsMask(ipos,mask);
//--------------------------------------------------------------
  // Optional color processing
  #ifdef CRTS_TONE
   // Tonal control, start by protecting from /0
   float peak=max(1.0/(256.0*65536.0),
    max3f(color.r,color.g,color.b));
   // Compute the ratios of {R,G,B}
   vec3 ratio=color*(1.0/(peak));
   // Apply tonal curve to peak value
   #ifdef CRTS_CONTRAST
    peak=pow(peak,tone.x);
   #endif
   peak=peak*(1.0/(peak*tone.y+tone.z));
   // Apply saturation
   #ifdef CRTS_SATURATION
    ratio=pow(ratio,vec3(tone.w,tone.w,tone.w));
   #endif
   // Reconstruct color
   return ratio*peak;
 #else
  return color;
 #endif
//--------------------------------------------------------------
 }

void main()
{
	vec2 warp_factor;
	warp_factor.x = CURVATURE;
	warp_factor.y = (3.0 / 4.0) * warp_factor.x; // assume 4:3 aspect
	warp_factor.x *= (1.0 - TRINITRON_CURVE);
	FragColor.rgb = CrtsFilter(vTexCoord.xy * OutputSize.xy,
	SourceSize.xy * OutputSize.zw,
	SourceSize.xy * vec2(0.5,0.5),
	SourceSize.zw,
	OutputSize.zw,
	2.0 * OutputSize.zw,
	SourceSize.y,
	warp_factor,
	INPUT_THIN,
	INPUT_BLUR,
	INPUT_MASK,
	CrtsTone(1.0,0.0,INPUT_THIN,INPUT_MASK));
	
	// Shadertoy outputs non-linear color
	FragColor.rgb = ToSrgb(FragColor.rgb);
}

@end

@program spritebatch_crt vs fs


