#pragma once

inline constexpr char SceneShader[] = R"(
#ifndef SEA_PRIMARY_AMPLITUDE
#define SEA_PRIMARY_AMPLITUDE 2.1
#define SEA_SECONDARY_AMPLITUDE .95
#define SEA_PRIMARY_FREQUENCY .025
#define SEA_SECONDARY_FREQUENCY .04
#define SEA_PRIMARY_SPEED 1.1
#define SEA_SECONDARY_SPEED .8
#endif
cbuffer Scene : register(b0) { row_major float4x4 mvp; row_major float4x4 model; float4 tint; float4 eye; float4 options; float4 fogColor; float4 fogRange; float4 sunDirection; float4 sunColor; };
Texture2D surface : register(t0); Texture2D detail : register(t1); SamplerState wrapSampler : register(s0);
struct V { float3 p:POSITION; float3 n:NORMAL; float2 uv:TEXCOORD; };
struct P { float4 p:SV_POSITION; float3 world:POSITION; float3 n:NORMAL; float2 uv:TEXCOORD; };
// Aynı dalga fonksiyonu konum ve normal için kullanılır; kürede dikiş oluşmaz.
// Use the same wave function for position and normal to avoid seams on the sphere.
float3 WaveGradient(float3 p,float time,out float height)
{
    float3 a=normalize(float3(1,.2,.7)),b=normalize(float3(-.4,.3,1));
    float first=dot(p,a)*SEA_PRIMARY_FREQUENCY-time*SEA_PRIMARY_SPEED;
    float second=dot(p,b)*SEA_SECONDARY_FREQUENCY+time*SEA_SECONDARY_SPEED;
    height=sin(first)*SEA_PRIMARY_AMPLITUDE+sin(second)*SEA_SECONDARY_AMPLITUDE;
    return a*cos(first)*(SEA_PRIMARY_FREQUENCY*SEA_PRIMARY_AMPLITUDE)+b*cos(second)*(SEA_SECONDARY_FREQUENCY*SEA_SECONDARY_AMPLITUDE);
}
P VS(V v)
{
    if(options.y>.5 && options.y<1.5)
    {
        float height;
        float3 up=normalize(v.p),gradient=WaveGradient(v.p,options.z,height);
        v.p+=up*height;
        v.n=normalize(up-gradient+up*dot(gradient,up));
    }
    P o;o.p=mul(float4(v.p,1),mvp);o.world=mul(float4(v.p,1),model).xyz;
    o.n=mul(float4(v.n,0),model).xyz;o.uv=v.uv;return o;
}
float4 PS(P p):SV_TARGET {
    float3 n=normalize(p.n);
    if(options.y>1.5 && options.y<2.5) {
        float3 dx=ddx(p.world), dy=ddy(p.world); float2 ux=ddx(p.uv), uy=ddy(p.uv);
        float determinant=ux.x*uy.y-ux.y*uy.x;
        if(abs(determinant)>1e-8) {
            float3 t=(dx*uy.y-dy*ux.y)/determinant;
            float3 b=(-dx*uy.x+dy*ux.x)/determinant;
            float3 mapped=detail.Sample(wrapSampler,p.uv).xyz*2-1;
            // UV dikişlerinde sıfıra yakın türevler oluşabilir.
            // Derivatives may become degenerate at UV seams.
            if(dot(t,t)>1e-10 && dot(b,b)>1e-10)
                n=normalize(mapped.x*normalize(t)+mapped.y*normalize(b)+mapped.z*n);
        }
    }
    if(options.y>.5 && options.y<1.5)
    {
        // Küçük dalgalar piksel normalinde kalır; uzakta geometriyi titreştirmez.
        // Keep small ripples in the pixel normal to avoid distant geometry shimmer.
        float3 ripple=float3(cos(p.world.x*.22+options.z*1.6),cos(p.world.y*.19-options.z),sin(p.world.z*.24+options.z*1.3))*.045;
        n=normalize(n+ripple-n*dot(ripple,n));
    }
    float diffuse=saturate(dot(n,sunDirection.xyz));
    float3 light=sunColor.w*lerp(float3(.7,.78,1),float3(.85,.94,1),sunDirection.w)+diffuse*sunColor.rgb*.85;
    float3 color=surface.Sample(wrapSampler,p.uv*options.x+(options.y>.5 && options.y<1.5?float2(options.z*.004,-options.z*.003):float2(0,0))).rgb*tint.rgb;
    if(options.y<.5) {
        float slope=1-saturate(dot(n,normalize(p.world)));
        float3 weights=pow(abs(n),4);weights/=max(dot(weights,1),.0001);
        float3 q=p.world/100;
        float3 grass=surface.Sample(wrapSampler,q.yz).rgb*weights.x+surface.Sample(wrapSampler,q.xz).rgb*weights.y+surface.Sample(wrapSampler,q.xy).rgb*weights.z;
        float3 rock=detail.Sample(wrapSampler,q.yz).rgb*weights.x+detail.Sample(wrapSampler,q.xz).rgb*weights.y+detail.Sample(wrapSampler,q.xy).rgb*weights.z;
        color=lerp(grass,rock,smoothstep(.12,.6,slope))*tint.rgb;
    }
    if(options.y>.5 && options.y<1.5)
    {
        float3 viewDirection=normalize(eye.xyz-p.world);
        float fresnel=pow(1-saturate(dot(n,viewDirection)),4);
        color=lerp(color,fogColor.rgb*.7,fresnel*.38);
        float sparkle=pow(saturate(dot(reflect(-sunDirection.xyz,n),viewDirection)),64);
        color+=sparkle*sunColor.rgb*.8;
    }
    // Mesafe tabanlı sis, parça sınırlarını gökyüzü rengine karıştırır.
    // Distance fog blends the streamed boundary into the sky color.
    float distanceToEye=length(p.world-eye.xyz);
    float fog=fogRange.z*smoothstep(fogRange.x,fogRange.y,distanceToEye);
    return float4(lerp(color*light,fogColor.rgb,fog),1);
}

struct SkyPixel
{
    float4 position : SV_POSITION;
    float3 ray : TEXCOORD0;
    float2 planeUV : TEXCOORD1;
};
SkyPixel SkyVS(uint id : SV_VertexID)
{
    // Altı düz yüzey gökyüzünü kapatır; doku her yüzeyde küçük karolarla tekrarlanır.
    // Six flat faces enclose the sky; each face repeats the texture in small tiles.
    const float2 corners[6]={float2(0,0),float2(0,1),float2(1,1),float2(0,0),float2(1,1),float2(1,0)};
    float2 uv=corners[id%6],p=uv*2-1;
    uint face=id/6;
    float3 local;
    if(face==0) local=float3(p.x,p.y,1);
    else if(face==1) local=float3(-p.x,p.y,-1);
    else if(face==2) local=float3(1,p.y,-p.x);
    else if(face==3) local=float3(-1,p.y,p.x);
    else if(face==4) local=float3(p.x,1,-p.y);
    else local=float3(p.x,-1,p.y);
    SkyPixel output;
    output.ray=mul(float4(local,0),model).xyz;
    output.position=mul(float4(output.ray,0),mvp);
    output.position.z=output.position.w;
    output.planeUV=uv*options.xy;
    return output;
}
float3 StarSample(float2 uv,float2 imageSize)
{
    // Aynalı tekrar kenarları birleştirir; görüntüyü büyütmeden doğal piksel yoğunluğunu koru.
    // Mirrored tiling joins image edges while keeping roughly native pixel density.
    uv=1-abs(frac(uv*.5)*2-1);
    uv=clamp(uv,.5/imageSize,1-.5/imageSize);
    return surface.Sample(wrapSampler,uv).rgb;
}
float4 SkyPS(SkyPixel input) : SV_TARGET
{
    float3 ray=normalize(input.ray),up=normalize(eye.xyz);
    float elevation=dot(ray,up),day=sunDirection.w;
    float sunHeight=dot(up,sunDirection.xyz);
    float twilight=exp(-pow(sunHeight/.19,2));
    float facingSun=pow(saturate(dot(ray,sunDirection.xyz)*.5+.5),6);
    float3 zenith=lerp(float3(.018,.032,.065),float3(.08,.34,.68),day);
    float horizon=pow(1-saturate(elevation),3);
    float3 sky=lerp(zenith,fogColor.rgb,horizon);
    uint width,height;
    surface.GetDimensions(width,height);
    float2 imageSize=float2(max(width,1u),max(height,1u));
    float3 stars=StarSample(input.planeUV,imageSize);
    float night=1-smoothstep(0,.75,day);
    // Siyah doku pikselleri gökyüzünün ortam rengini silmesin.
    // Black texture pixels must not replace the sky's ambient color.
    sky+=stars*.7*night;
    sky+=float3(.65,.16,.025)*twilight*facingSun*pow(1-saturate(abs(elevation)),3);
    float alignment=dot(ray,sunDirection.xyz);
    float disc=smoothstep(.99955,.99985,alignment);
    float halo=exp((alignment-1)*110)*.3;
    float visible=smoothstep(-.04,.03,sunHeight);
    sky+=(disc+halo)*lerp(float3(1,.32,.06),float3(1,.96,.78),saturate(sunHeight*3))*visible;
    return float4(saturate(sky),1);
}
)";
