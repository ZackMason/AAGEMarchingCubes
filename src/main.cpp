
#define WIN32_MEAN_AND_LEAN
#define NOMINMAX

#include "core.hpp"

#include <glad/glad.h>

#include "imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "Engine/window.hpp"
#include "Engine/orbit_camera.hpp"
#include "Engine/asset_loader.hpp"

#include "Graphics/buffer.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/vertex_array.hpp"
#include "Graphics/batch2d.hpp"
#include "Graphics/font.hpp"

#include "Util/string.hpp"
#include "Util/allocators.hpp"
#include "Util/color.hpp"
#include "Util/console.hpp"
#include "Util/timer.hpp"
#include "Util/random.hpp"

#include "marching_cubes.hpp"

#include <thread>
#include <span>
#include <ranges>


v3f tri_norm(const v3f& a, const v3f& b, const v3f& c) {
    return glm::normalize(glm::cross(b-a, c-a));
}

f32 hash1(const v3f& v) {
    return glm::fract(glm::sin(glm::dot(v,
                        v3f(12.9898f,78.233f, 234.2412f)))*
        43758.5453123f);
}



v3f hash(const v3f& v) {
    return v3f{
        hash1(v),
        hash1(v * 35.324f),
        hash1(v * -123.3423f)
    };
}

// returns 3D value noise
float noise( const v3f& x )
{
    // grid
    const v3f p = floor(x);
    const v3f w = fract(x);
    
    // quintic interpolant
    const v3f u = w*w*w*(w*(w*6.0f-15.0f)+10.0f);

    // gradients
    const v3f ga = hash( p+v3f(0.0f,0.0f,0.0f) );
    const v3f gb = hash( p+v3f(1.0f,0.0f,0.0f) );
    const v3f gc = hash( p+v3f(0.0f,1.0f,0.0f) );
    const v3f gd = hash( p+v3f(1.0f,1.0f,0.0f) );
    const v3f ge = hash( p+v3f(0.0f,0.0f,1.0f) );
    const v3f gf = hash( p+v3f(1.0f,0.0f,1.0f) );
    const v3f gg = hash( p+v3f(0.0f,1.0f,1.0f) );
    const v3f gh = hash( p+v3f(1.0f,1.0f,1.0f) );
    
    // projections
    float va = glm::dot( ga, w-v3f(0.0f,0.0f,0.0f) );
    float vb = glm::dot( gb, w-v3f(1.0f,0.0f,0.0f) );
    float vc = glm::dot( gc, w-v3f(0.0f,1.0f,0.0f) );
    float vd = glm::dot( gd, w-v3f(1.0f,1.0f,0.0f) );
    float ve = glm::dot( ge, w-v3f(0.0f,0.0f,1.0f) );
    float vf = glm::dot( gf, w-v3f(1.0f,0.0f,1.0f) );
    float vg = glm::dot( gg, w-v3f(0.0f,1.0f,1.0f) );
    float vh = glm::dot( gh, w-v3f(1.0f,1.0f,1.0f) );
	
    // interpolation
    return va + 
           u.x*(vb-va) + 
           u.y*(vc-va) + 
           u.z*(ve-va) + 
           u.x*u.y*(va-vb-vc+vd) + 
           u.y*u.z*(va-vc-ve+vg) + 
           u.z*u.x*(va-vb-ve+vf) + 
           u.x*u.y*u.z*(-va+vb+vc-vd+ve-vf-vg+vh);
}

float o_noise(const v3f& v, int oct, f32 s = 1.0f) {
    if (oct) return noise(v*s) + o_noise(v, oct-1, s*0.5f);
    return 0.0f;
}

float random2f(const v2f& st) {
    return glm::fract(glm::sin(glm::dot(st,
                         v2f(12.9898f,78.233f)))*
        43758.5453123f);
}

v2f cell_noise( const v2f& x )
{
    const v2f p = glm::floor( x );
    const v2f f = glm::fract( x );

    v2f res = v2f( 8.0f );
    for( int j=-1; j<=1; j++ )
        for( int i=-1; i<=1; i++ ) {
            const v2f b = v2f(i, j); 
            const v2f  r = v2f(b) - f + random2f(p + b);
            const float d = dot(r, r);

            if( d < res.x )
            {
                res.y = res.x;
                res.x = d;
            }
            else if( d < res.y )
            {
                res.y = d;
            }
    }

    return glm::sqrt( res );
}


int main(int argc, char** argv) {
    random_s::randomize();
    window_t window;
    window.width = 640;
    window.height = 480;

    window.set_event_callback([](auto& event){
        event_handler_t dispatcher;

        dispatcher.dispatch<const char_event_t>(event, [](const char_event_t& e){
            if (console_t::get().is_open()) {
                if (e.codepoint == '[' || e.codepoint == ']' || e.codepoint == '`') return true;
                console_t::get().input_text += static_cast<char>(e.codepoint);
            }
            return true; 
        });

        dispatcher.dispatch<const key_event_t>(event, [](const key_event_t& e){
            console_t::get().on_key_event(e);
            if (e.key == 257) { // enter
                console_t::get().on_enter(nullptr);
            }
            return true;
        });
    });

    window.open_window();
    window.set_title("AAGE Marching Cubes");
    window.

    const std::string asset_dir = "./assets/";
    asset_loader_t loader{};
    loader.asset_dir = asset_dir;

    const auto font_path = fmt::format("{}/fonts/open_sans.ttf", GAME_ASSETS_PATH);
    font_t font{font_path, 22.0f};
    batch2d_t<5000> ctx_2d{};

    bool wire_frame = false;
    console_t::get().commands.emplace_back(
        "wireframe"_sid, [&](void* _d, auto& args) {
            wire_frame = !wire_frame;
            glPolygonMode(GL_FRONT_AND_BACK, wire_frame ? GL_LINE : GL_FILL );
        }, 0
    );

    ctx_2d.shader = loader.get_shader_nameless("shaders/batch.vs", "shaders/batch.fs");

    stack_allocator_t allocator{1024*1024};

    v3f color{0.3f, 0.5f, 1.0f};

    orbit_camera_t camera{45.0f, 1.0f, 1.0f, 0.1f, 700.0f};
    timer32_t timer;

    struct vert_t {
        v3f p;
        v3f n;
        v3f c;
    };

    constexpr size_t v_max_count = 1'000'000;
    size_t v_count = 0;
    mapped_buffer_t<vert_t, v_max_count> vertices;
    vertex_array_t vertex_array{vertices.id, 0, sizeof(vert_t)};
    vertex_array
        .set_attrib(0, 3, GL_FLOAT, offsetof(vert_t, p))
        .set_attrib(1, 3, GL_FLOAT, offsetof(vert_t, n))
        .set_attrib(2, 3, GL_FLOAT, offsetof(vert_t, c));

    std::span<vert_t, v_max_count> buffer{vertices.get_data(), v_max_count};

    const auto make_terrain = [&](){
        v_count = 0;
        for (f32 x = 0; x < 2.0f*64.0f; x += 1.0f) {
            for (f32 y = 0; y < 32.0f; y += 1.0f) {
                for (f32 z = 0; z < 2.0f*64.0f; z += 1.0f) {
                    marching_cubes::grid_cell_t g;
                    g.p[0] = v3f{x,y,z};
                    g.p[1] = v3f{x,y,z+1};
                    g.p[2] = v3f{x+1,y,z+1};
                    g.p[3] = v3f{x+1,y,z};

                    g.p[4] = v3f{x,y+1,z};
                    g.p[5] = v3f{x,y+1,z+1};
                    g.p[6] = v3f{x+1,y+1,z+1};
                    g.p[7] = v3f{x+1,y+1,z};
                    for (int i = 0; i < 8; i++) {
                        g.p[i] *= 7.0f;
                        //g.v[i] = glm::length(g.p[i] - v3f{5.0f}) - 5.0f;
                        g.v[i] = o_noise(g.p[i] * 0.025f,4) * 0.5f;
                        g.v[i] += (8.0f - g.p[i].y) * 0.01f;
                        g.v[i] += cell_noise(v2f{g.p[i].x, g.p[i].z}*0.02f).x * 0.25f;
                    }
                    marching_cubes::triangle_t tris[32];
                    int tri_count = marching_cubes::to_polygon(g, 0.0f, tris);

                    for (int i = 0; i < tri_count; i++) {
                        buffer[v_count + 0].p = tris[i].p[0];
                        buffer[v_count + 1].p = tris[i].p[1];
                        buffer[v_count + 2].p = tris[i].p[2];

                        const v3f n = tri_norm(tris[i].p[0], tris[i].p[1], tris[i].p[2]);

                        buffer[v_count + 0].n = n;
                        buffer[v_count + 1].n = n;
                        buffer[v_count + 2].n = n;
                        
                        const auto c = [&]() -> v3f {
                            return lerp(
                                v3f{0.2157, 0.1451, 0.1451}, 
                                v3f{0.2784, 0.3529, 0.1882},
                                std::max(glm::dot(n, v3f{random_s::randn()*0.2f,1.0f,random_s::randn()*0.2f}),0.0f)
                            );
                        }; 

                        buffer[v_count + 0].c = c() + v3f{random_s::randf() * 0.1f};
                        buffer[v_count + 1].c = c() + v3f{random_s::randf() * 0.1f};
                        buffer[v_count + 2].c = c() + v3f{random_s::randf() * 0.1f};
                        v_count += 3;
                    }
                }
            }
        }
    };
    const auto make_sphere = [&](){
        v_count = 0;
        for (f32 x = 0; x < 2.0f*64.0f; x += 1.0f) {
            for (f32 y = 0; y < 32.0f; y += 1.0f) {
                for (f32 z = 0; z < 2.0f*64.0f; z += 1.0f) {
                    marching_cubes::grid_cell_t g;
                    g.p[0] = v3f{x,y,z};
                    g.p[1] = v3f{x,y,z+1};
                    g.p[2] = v3f{x+1,y,z+1};
                    g.p[3] = v3f{x+1,y,z};

                    g.p[4] = v3f{x,y+1,z};
                    g.p[5] = v3f{x,y+1,z+1};
                    g.p[6] = v3f{x+1,y+1,z+1};
                    g.p[7] = v3f{x+1,y+1,z};
                    for (int i = 0; i < 8; i++) {
                        g.p[i] *= 2.0f;
                        g.v[i] = glm::length(g.p[i] - v3f{25.0f}) - 25.0f;
                    }
                    marching_cubes::triangle_t tris[32];
                    int tri_count = marching_cubes::to_polygon(g, 0.0f, tris);

                    for (int i = 0; i < tri_count; i++) {
                        buffer[v_count + 0].p = tris[i].p[0];
                        buffer[v_count + 1].p = tris[i].p[1];
                        buffer[v_count + 2].p = tris[i].p[2];

                        const v3f n = tri_norm(tris[i].p[0], tris[i].p[1], tris[i].p[2]);

                        buffer[v_count + 0].n = n;
                        buffer[v_count + 1].n = n;
                        buffer[v_count + 2].n = n;
                        
                        const auto c = [&]() -> v3f {
                            return lerp(
                                v3f{0.2157, 0.1451, 0.1451}, 
                                v3f{0.2784, 0.3529, 0.1882},
                                std::max(glm::dot(n, v3f{random_s::randn()*0.2f,1.0f,random_s::randn()*0.2f}),0.0f)
                            );
                        }; 

                        buffer[v_count + 0].c = c() + v3f{random_s::randf() * 0.1f};
                        buffer[v_count + 1].c = c() + v3f{random_s::randf() * 0.1f};
                        buffer[v_count + 2].c = c() + v3f{random_s::randf() * 0.1f};
                        v_count += 3;
                    }
                }
            }
        }
    };
    const auto make_tiles = [&](){
        v_count = 0;
        for (f32 x = 0; x < 2.0f*64.0f; x += 1.0f) {
            for (f32 y = 0; y < 32.0f; y += 1.0f) {
                for (f32 z = 0; z < 2.0f*64.0f; z += 1.0f) {
                    
                    marching_cubes::grid_cell_t g;
                    g.p[0] = v3f{x,y,z};
                    g.p[1] = v3f{x,y,z+1};
                    g.p[2] = v3f{x+1,y,z+1};
                    g.p[3] = v3f{x+1,y,z};

                    g.p[4] = v3f{x,y+1,z};
                    g.p[5] = v3f{x,y+1,z+1};
                    g.p[6] = v3f{x+1,y+1,z+1};
                    g.p[7] = v3f{x+1,y+1,z};
                    for (int i = 0; i < 8; i++) {
                        #if 1
                        const auto p = glm::floor(g.p[i] / 20.0f);

                        f32 xx = noise(p);
                        g.v[i] = (xx > glm::clamp(g.p[i].y - 5.0f,0.0f, 1.0f));

                        #else
                        const f32 ts = 1.0f/5.0f;
                        const auto p = g.p[i] / 20.0f * v3f{ts,1.0f,ts};
                        f32 xx = noise(p) + 0.1f; 
                        g.v[i] = (xx > glm::clamp((g.p[i].y - 5.0f)/20.0f, 0.0f, 1.0f));
                        #endif
                        //g.v[i] = (xx > 0.50f * g.p[i].y - 1.0f);
                    }
                    marching_cubes::triangle_t tris[32];
                    int tri_count = marching_cubes::to_polygon(g, 0.5f, tris);

                    for (int i = 0; i < tri_count; i++) {
                        buffer[v_count + 0].p = tris[i].p[0];
                        buffer[v_count + 1].p = tris[i].p[1];
                        buffer[v_count + 2].p = tris[i].p[2];

                        const v3f n = tri_norm(tris[i].p[0], tris[i].p[1], tris[i].p[2]);

                        buffer[v_count + 0].n = n;
                        buffer[v_count + 1].n = n;
                        buffer[v_count + 2].n = n;
                        
                        const auto c = [&]() -> v3f {
                            return lerp(
                                v3f{0.2157, 0.1451, 0.1451}, 
                                v3f{0.2784, 0.3529, 0.1882},
                                std::max(glm::dot(n, v3f{random_s::randn()*0.2f,1.0f,random_s::randn()*0.2f}),0.0f)
                            );
                        }; 

                        buffer[v_count + 0].c = c() + v3f{random_s::randf() * 0.1f};
                        buffer[v_count + 1].c = c() + v3f{random_s::randf() * 0.1f};
                        buffer[v_count + 2].c = c() + v3f{random_s::randf() * 0.1f};
                        v_count += 3;
                    }
                }
            }
        }
    };

    console_t::get().commands.push_back(
        console_t::command_t{"terrain",
        [&](auto* d, auto& args){
            std::thread{make_terrain}.detach();
        }}
    );
    
    console_t::get().commands.push_back(
        console_t::command_t{"sphere",
        [&](auto* d, auto& args){
            std::thread{make_sphere}.detach();
        }}
    );
    console_t::get().commands.push_back(
        console_t::command_t{"tiles",
        [&](auto* d, auto& args){
            std::thread{make_tiles}.detach();
        }}
    );
    
    console_t::get().input_text = "tiles";
    console_t::get().on_enter(0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    glEnable(GL_DEPTH_TEST);  
    // glCullFace(GL_BACK);

    while(!window.should_close()) {
        glClearColor(color.r, color.g, color.b, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        /////////////////////////////
        const auto dt = timer.get_dt(window.get_ticks());
        if (console_t::get().is_open() == false) {
            camera.update(window, dt);
        }

        /////////////////////////////
        console_t::get().screen_size = ctx_2d.screen_size = window.size();
        ctx_2d.clear();

        console_t::get().draw(ctx_2d, font);


        ctx_2d.present();
        /////////////////////////////
        const auto terrain_shader = loader.get_shader_nameless("shaders/terrain.vs", "shaders/terrain.fs");
        assert(terrain_shader.valid());

        terrain_shader.get().bind();
        terrain_shader.get().set_mat4("uVP", camera.view_projection());

        glDisable(GL_CULL_FACE);    
        vertex_array.size = v_count;
        vertex_array.draw();

        /////////////////////////////
        window.poll_events();
        window.swap_buffers();
    }

    return 0;
}