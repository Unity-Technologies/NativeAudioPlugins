// Upgrade NOTE: replaced 'mul(UNITY_MATRIX_MVP,*)' with 'UnityObjectToClipPos(*)'

// Small 3D fractal raytracer running in a full-screen pixel shader
// Distance estimation is used to traverse space from the eye to the intersection point -- for more information on this topic see http://iquilezles.org/www/
// The basic distance function used is a Sierpinski triangle (see comment below)
// Post-processing effects for flickering, noise and scanline distortion are built into the shader.
// The exposed properties allow the driver script to animate the rendering according to the parameters used to drive the granular sound synthesis.

Shader "Custom/fractal"
{
    Properties
    {
        _Offset ("Offset", Range (0.0, 1.0)) = 0.0
        _RndOffset ("RndOffset", Range (0.0, 1.0)) = 0.0
        _RndSpeed ("RndSpeed", Range (0.0, 1.0)) = 0.0
    }

    SubShader
    {
        Pass
        {
            CGPROGRAM

            #pragma target 3.0
            #pragma vertex vert
            #pragma fragment frag

            #include "UnityCG.cginc"

            struct vertOut
            {
                float4 pos:SV_POSITION;
                float4 scrPos:COLOR;
            };

            vertOut vert(in appdata_base v)
            {
                vertOut o;
                o.pos = UnityObjectToClipPos (v.vertex);
                o.scrPos = ComputeScreenPos(o.pos);
                return o;
            }

            float smin2(float a, float b, float k) { float res = exp(-k * a) + exp(-k * b); return -log(res) / k; }
            float smin3(float a, float b, float c, float k) { float res = exp(-k * a) + exp(-k * b) + exp(-k * c); return -log(res) / k; }

            float Offset;
            float RndOffset;
            float RndSpeed;

            float4 f(float3 p)
            {
                float a1 = 0.3, c1 = cos(a1), s1 = sin(a1);
                float r, iter = 0.0, scale = 1.55, col = 0.0;
                for(int i = 0; i < 16; i++)
                {
                    float3 q = max(abs(p) - 0.2, 0.0) - 0.3;
                    r = dot(q, q);
                    if(r >= 500.0)
                        break;
                    if(p.x + p.y < 0.0) { float tmp = -p.y; p.y = -p.x; p.x = tmp; col += 0.1; }
                    if(p.x + p.z < 0.0) { float tmp = -p.z; p.z = -p.x; p.x = tmp; col += 0.1; }
                    if(p.y + p.z < 0.0) { float tmp = -p.z; p.z = -p.y; p.y = tmp; col += 0.1; }
                    p = p * scale - scale * 0.5;
                    p = float3(p.x * c1 - p.z * s1, p.y, p.x * s1 + p.z * c1); // Comment this line out and set scale to 2.0 to see Sierpinski triangle
                    iter += 1.0;
                }
                float t = (sqrt(r) - 1.0) * pow(scale, -iter);
                float3 c = float3(0.9 + 0.1 * cos(col * 7.0), 0.25 + 0.2 * sin(_Time.x + col * 2.0), 0.1 + 0.1 * sin(_Time.x + col * 3.0));
                return float4(c, t);
            }

            fixed4 frag (vertOut i) : COLOR
            {
                float2 uv = i.scrPos.xy - 0.5;
                float distort = sin(_Time.x * 0.3 + 293.0 * uv.y) * pow(0.7 + 0.3 * sin(_Time.x * 10.6 + 0.29 * uv.y + 7.0 * sin(7.29 * uv.y)), 223.0) * 0.011;
                uv.y *= _ScreenParams.y / _ScreenParams.x;
                uv.x += sin(_Time.x + 33.0 * uv.y) * 0.005 + distort;
                float3 p, p0 = float3(2.0, 2.0, -3.0), dir0 = normalize(float3(uv, 1.0));
                float a1 = 7.0 * sin(_Time.x * 0.041);
                float a2 = 7.0 * sin(_Time.x * 0.071);
                float c1 = cos(a1), s1 = sin(a1);
                float c2 = cos(a2), s2 = sin(a2);
                float r = 0.3 * Offset - 0.1;
                float3 dir1 = float3(dir0.x * c1 - dir0.z * s1, dir0.y, dir0.x * s1 + dir0.z * c1);
                float3 dir = float3(dir1.x, dir1.y * c2 - dir1.z * s2, dir1.y * s2 + dir1.z * c2);
                p0 = float3(-r * s1, 0.0, r * c1);
                p0 = float3(p0.x, p0.y * c2 - p0.z * s2, p0.y * s2 + p0.z * c2);
                p0 += dir * 0.005;
                float t = 0.0;
                float4 d = float4(0.0, 0.0, 0.0, 0.0);
                for(int i = 0; i < 50; i++)
                {
                    p = p0 + t * dir;
                    d = f(p);
                    if(d.w < 0.0003)
                        break;
                    t += d.w;
                }
                float c = 1.0 - 0.003 * t, eps = 0.01;
                float c0 = f(float3(p.x, p.y, p.z)).w;
                float cx = f(float3(p.x+eps, p.y, p.z)).w;
                float cy = f(float3(p.x, p.y+eps, p.z)).w;
                float cz = f(float3(p.x, p.y, p.z+eps)).w;
                float3 n = normalize(float3(cx-c0, cy-c0, cz-c0));
                float flicker = sin(30000.0 * _Time * (RndOffset + RndSpeed));
                c *= 0.5 + dot(n, float3(1.0, 1.0, 1.0)) * 0.3;
                c = c / (t * 1.95 + 1.5);
                c *= 1.0 + 1.0 * (Offset + RndSpeed);
                c *= 1.0 - 0.02 * flicker - 0.03 * length(3.0 * uv) * flicker;
                c *= 0.8 + 0.2 * abs(sin(1000.0 * _Time.x + uv.y * 501.0));
                c *= min(_Time.x * 2.0, 1.0);
                return float4(c * d.xyz * sin(c * 2.5 + distort * float3(2.0, 32.1, 1211.7)), 1.0);
            }

            ENDCG
        }
    }
}
