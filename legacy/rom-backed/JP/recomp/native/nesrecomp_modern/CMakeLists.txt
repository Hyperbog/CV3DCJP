cmake_minimum_required(VERSION 3.20)
project(CVIII_Modern_NESRecomp C)

# Separate compatibility target. It never patches the upstream NESRecomp tree.
set(NESRECOMP_EXTERNAL_ROOT "" CACHE PATH "Existing upstream mstan/nesrecomp checkout")
set(JP_ORIGINAL_JP_ROM "" CACHE FILEPATH "Private authenticated Japanese ROM")
set(JP_GAME_CONFIG "" CACHE FILEPATH "Optional game.toml")
option(JP_CYCLE_HEADLESS "Compile without SDL2 window/audio" OFF)
option(JP_COLLISION_V11_SHADOW "Opt-in read-only CPU dispatch shadow; requires hook-aware game.toml" OFF)

if(NOT EXISTS "${NESRECOMP_EXTERNAL_ROOT}/runner/cyc/project.cmake")
    message(FATAL_ERROR "Set NESRECOMP_EXTERNAL_ROOT to your existing NESRecomp checkout")
endif()
if(NOT EXISTS "${JP_ORIGINAL_JP_ROM}")
    message(FATAL_ERROR "Set JP_ORIGINAL_JP_ROM to the Japanese retail .nes file")
endif()

# Match the independently recorded Japanese VRC6a cartridge exactly.
file(SIZE "${JP_ORIGINAL_JP_ROM}" JP_ROM_SIZE)
file(SHA256 "${JP_ORIGINAL_JP_ROM}" JP_ROM_SHA256)
set(JP_EXPECTED_SHA256 "958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28")
if(NOT JP_ROM_SIZE EQUAL 393232 OR NOT JP_ROM_SHA256 STREQUAL JP_EXPECTED_SHA256)
    message(FATAL_ERROR "Refusing different or modified ROM: expected Japanese Mapper 24 SHA256 ${JP_EXPECTED_SHA256}")
endif()
if(JP_COLLISION_V11_SHADOW AND NOT JP_GAME_CONFIG)
    message(FATAL_ERROR "v11 shadow requires owner-local game.toml with both explicitly declared [[mod_function_hook]] sites")
endif()
if(JP_GAME_CONFIG AND NOT EXISTS "${JP_GAME_CONFIG}")
    message(FATAL_ERROR "JP_GAME_CONFIG was supplied but the file does not exist")
endif()

# Opt in only when a compatible recomp-ui checkout is explicitly supplied.
# The UI owns the existing save browser/rewind; CVIII supplies settings rows.
option(JP_MODERN_ENABLE_UI "Use NESRecomp/recomp-ui save browser and CVIII settings" ON)
if(JP_MODERN_ENABLE_UI)
    if(JP_CYCLE_HEADLESS)
        message(FATAL_ERROR "JP_MODERN_ENABLE_UI requires a windowed build")
    endif()
    if(NOT NESRECOMP_RECOMP_UI OR NESRECOMP_RECOMP_UI STREQUAL "OFF"
       OR NOT EXISTS "${NESRECOMP_RECOMP_UI}/src/recomp_runtime_ui.h")
        message(FATAL_ERROR "Set NESRECOMP_RECOMP_UI to a compatible recomp-ui checkout")
    endif()
else()
    set(NESRECOMP_RECOMP_UI OFF CACHE STRING "No launcher without opt-in" FORCE)
endif()
include("${NESRECOMP_EXTERNAL_ROOT}/runner/cyc/project.cmake")
if(JP_MODERN_ENABLE_UI AND NOT NESRECOMP_CYC_RECOMP_UI_ROOT)
    message(FATAL_ERROR "Requested recomp-ui is incompatible with this NESRecomp revision")
endif()

set(JP_AUDIO_DIR "${CMAKE_CURRENT_LIST_DIR}/../enhanced")
foreach(_f cviii_audio_levels_0618.h cviii_audio_levels_0618.c
           cviii_sprite_layers_0608.h cviii_sprite_layers_0608.c
           cviii_sprite_layers_test_0608.c
           cviii_chr_index_0598.h cviii_chr_index_0598.c
           cviii_whole_room.h cviii_whole_room.c
           cviii_room_gfx.h cviii_room_gfx.c
           cviii_room_renderer.h cviii_room_renderer.c
           cviii_room_preview.h cviii_room_preview.c)
    if(NOT EXISTS "${JP_AUDIO_DIR}/${_f}")
        message(FATAL_ERROR "Original CVIII JP0618 module missing: ${JP_AUDIO_DIR}/${_f}")
    endif()
endforeach()

# Preserve the original game CPU, PPU, APU and VRC6 implementations.
# Only a supported, presentation-side host callback is linked.
set(JP_MODERN_HOST_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/cviii_modern_host.c"
    "${JP_AUDIO_DIR}/cviii_audio_levels_0618.c"
    "${JP_AUDIO_DIR}/cviii_sprite_layers_0608.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_room_diag.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_native_world_5h2.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_world_parity_5h5.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_ppu_audit_5h6.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_track_5h7.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_room_local_5h8.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_scene_epoch_5h8c.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_dual_scene_5h8c.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_transition_5h8d.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_mutable_tiles_5h8e.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_entity_events_5h8f.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_boss_cache_5h8c.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_trace_5h3.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_gate_5h4.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_ppu_guard_5g2.c"
    "${JP_AUDIO_DIR}/cviii_chr_index_0598.c"
    "${JP_AUDIO_DIR}/cviii_whole_room.c"
    "${JP_AUDIO_DIR}/cviii_room_gfx.c"
    "${JP_AUDIO_DIR}/cviii_room_renderer.c"
    "${JP_AUDIO_DIR}/cviii_room_preview.c")
set(JP_GAME_OPTIONS "")
if(NOT JP_MODERN_ENABLE_UI)
    list(APPEND JP_GAME_OPTIONS NO_RECOMP_UI)
endif()
if(JP_CYCLE_HEADLESS)
    list(APPEND JP_GAME_OPTIONS HEADLESS)
endif()
if(JP_GAME_CONFIG)
    list(APPEND JP_GAME_OPTIONS GAME_CONFIG "${JP_GAME_CONFIG}")
endif()

nesrecomp_add_cycle_game(jp_akumajou_densetsu_modern
    ROM "${JP_ORIGINAL_JP_ROM}"
    HOST_EXTRAS ${JP_MODERN_HOST_SOURCES}
    ${JP_GAME_OPTIONS})
# v11 opt-in: only a read-only CPU dispatch observation; never replaces code.
# Preserve the default game, tests, and upstream checkout unchanged.
if(JP_COLLISION_V11_SHADOW)
    set(JP_COLLISION_V11_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../../../src/collision")
    target_sources(jp_akumajou_densetsu_modern PRIVATE
        "${JP_COLLISION_V11_DIR}/Collision.c"
        "${JP_COLLISION_V11_DIR}/CollisionLive.c"
        "${JP_COLLISION_V11_DIR}/CollisionDispatchV11.c")
    target_include_directories(jp_akumajou_densetsu_modern PRIVATE
        "${JP_COLLISION_V11_DIR}"
        "${NESRECOMP_EXTERNAL_ROOT}/runner/include")
    target_compile_definitions(jp_akumajou_densetsu_modern PRIVATE
        CVIII_COLLISION_V11_SHADOW=1)
endif()

# The 5H8S/5H8U selector observer functions rely on private cycle-engine
# function-pointer symbols absent from clean pinned mstan/nesrecomp.
# The read-only headless collision v11 diagnostic does not capture selector
# writes. Link explicit HOST-only disabled entry points rather than inventing
# engine callback pointers or editing the protected/original game build.
if(JP_COLLISION_V11_SHADOW AND JP_CYCLE_HEADLESS)
    target_sources(jp_akumajou_densetsu_modern PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/cviii_selector_unavailable_collision_v11.c")
else()
    target_sources(jp_akumajou_densetsu_modern PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/cviii_selector_native_5h8s.c"
        "${CMAKE_CURRENT_LIST_DIR}/cviii_selector_native_5h8u.c")
endif() # CVIII-5H8U
target_compile_definitions(jp_akumajou_densetsu_modern PRIVATE CVIII_SELECTOR_TRACE_5H8S)
# BEGIN CVIII-5H8S, game target only; existing tests are untouched.
target_sources(jp_akumajou_densetsu_modern PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/cviii_diagnostic_5h8.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_diagnostic_runtime_5h8.c")
target_include_directories(jp_akumajou_densetsu_modern PRIVATE
    "${JP_AUDIO_DIR}" "${NESRECOMP_EXTERNAL_ROOT}/runner/cyc")
if(JP_MODERN_ENABLE_UI)
    target_include_directories(jp_akumajou_densetsu_modern PRIVATE
        "${NESRECOMP_RECOMP_UI}/src")
endif()

if(NOT JP_CYCLE_HEADLESS)
    get_target_property(JP_HAS_SDL jp_akumajou_densetsu_modern NESRECOMP_CYC_SDL)
    if(NOT JP_HAS_SDL)
        message(FATAL_ERROR "SDL2 not found: a windowed compatibility build is required")
    endif()
endif()

# Small host-interface regression: tests edition mode and original JP0618 PCM.
enable_testing()
add_executable(cviii_modern_host_test
    "${CMAKE_CURRENT_LIST_DIR}/cviii_modern_host_test.c"
    ${JP_MODERN_HOST_SOURCES}
    "${CMAKE_CURRENT_LIST_DIR}/cviii_host_test_stubs.c")
target_compile_features(cviii_modern_host_test PRIVATE c_std_11)
target_include_directories(cviii_modern_host_test PRIVATE
    "${JP_AUDIO_DIR}" "${NESRECOMP_EXTERNAL_ROOT}/runner/cyc")
if(JP_MODERN_ENABLE_UI)
    target_compile_definitions(cviii_modern_host_test PRIVATE CYC_WITH_RECOMP_UI)
    target_include_directories(cviii_modern_host_test PRIVATE
        "${NESRECOMP_RECOMP_UI}/src")
endif()
if(MSVC)
    target_compile_options(cviii_modern_host_test PRIVATE /utf-8)
    target_compile_definitions(cviii_modern_host_test PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
target_sources(cviii_modern_host_test PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/cviii_diagnostic_stub_5h8.c")
add_test(NAME CVIII_Modern_Host_Contract COMMAND cviii_modern_host_test)

# Reuse the original project's JP0608 test without changing its source.
add_executable(cviii_sprite_layers_test_0608
    "${JP_AUDIO_DIR}/cviii_sprite_layers_0608.c"
    "${JP_AUDIO_DIR}/cviii_sprite_layers_test_0608.c")
target_compile_features(cviii_sprite_layers_test_0608 PRIVATE c_std_11)
target_include_directories(cviii_sprite_layers_test_0608 PRIVATE "${JP_AUDIO_DIR}")
add_test(NAME CVIII_JP0608_Sprite_Regression COMMAND cviii_sprite_layers_test_0608)

# ROM-free 426px center-preservation geometry test (no retail assets).
add_executable(cviii_room_diag_test_5f
    "${CMAKE_CURRENT_LIST_DIR}/cviii_room_diag_test_5f.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_room_diag.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_native_world_5h2.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_world_parity_5h5.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_ppu_audit_5h6.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_track_5h7.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_room_local_5h8.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_scene_epoch_5h8c.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_dual_scene_5h8c.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_transition_5h8d.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_mutable_tiles_5h8e.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_entity_events_5h8f.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_boss_cache_5h8c.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_trace_5h3.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_gate_5h4.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_ppu_guard_5g2.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_host_test_stubs.c"
    "${JP_AUDIO_DIR}/cviii_chr_index_0598.c"
    "${JP_AUDIO_DIR}/cviii_whole_room.c"
    "${JP_AUDIO_DIR}/cviii_room_gfx.c"
    "${JP_AUDIO_DIR}/cviii_room_renderer.c"
    "${JP_AUDIO_DIR}/cviii_room_preview.c")
target_include_directories(cviii_room_diag_test_5f PRIVATE
    "${JP_AUDIO_DIR}" "${NESRECOMP_EXTERNAL_ROOT}/runner/cyc")
target_compile_features(cviii_room_diag_test_5f PRIVATE c_std_11)
if(MSVC)
    target_compile_options(cviii_room_diag_test_5f PRIVATE /utf-8)
    target_compile_definitions(cviii_room_diag_test_5f PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
add_test(NAME CVIII_5F_Room_Diagnostic_Geometry COMMAND cviii_room_diag_test_5f)

# PPU projection + 426-pixel viewport regression using the actual host adapter.
add_executable(cviii_room_diag_test_5g
    "${CMAKE_CURRENT_LIST_DIR}/cviii_room_diag_test_5g.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_room_diag.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_native_world_5h2.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_world_parity_5h5.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_ppu_audit_5h6.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_track_5h7.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_room_local_5h8.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_scene_epoch_5h8c.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_dual_scene_5h8c.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_transition_5h8d.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_mutable_tiles_5h8e.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_entity_events_5h8f.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_boss_cache_5h8c.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_trace_5h3.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_gate_5h4.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_ppu_guard_5g2.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_host_test_stubs.c"
    "${JP_AUDIO_DIR}/cviii_chr_index_0598.c"
    "${JP_AUDIO_DIR}/cviii_whole_room.c"
    "${JP_AUDIO_DIR}/cviii_room_gfx.c"
    "${JP_AUDIO_DIR}/cviii_room_renderer.c"
    "${JP_AUDIO_DIR}/cviii_room_preview.c")
target_include_directories(cviii_room_diag_test_5g PRIVATE
    "${JP_AUDIO_DIR}" "${NESRECOMP_EXTERNAL_ROOT}/runner/cyc")
target_compile_features(cviii_room_diag_test_5g PRIVATE c_std_11)
if(MSVC)
    target_compile_options(cviii_room_diag_test_5g PRIVATE /utf-8)
    target_compile_definitions(cviii_room_diag_test_5g PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
add_test(NAME CVIII_5G_Room_PPU_Extension COMMAND cviii_room_diag_test_5g)

# Release builds normally define NDEBUG, which would disable original assert-
# based tests. Explicitly undo it on tests, including historical JP0608.
foreach(_cviii_test cviii_modern_host_test cviii_sprite_layers_test_0608
                    cviii_room_diag_test_5f cviii_room_diag_test_5g)
    if(MSVC)
        target_compile_options(${_cviii_test} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${_cviii_test} PRIVATE -UNDEBUG)
    endif()
endforeach()

# Isolated synthetic PPU parity gate, without a user ROM or SDL.
add_executable(cviii_ppu_guard_test_5g2
    "${CMAKE_CURRENT_LIST_DIR}/cviii_ppu_guard_5g2.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_ppu_guard_test_5g2.c")
target_compile_features(cviii_ppu_guard_test_5g2 PRIVATE c_std_11)
if(MSVC)
    target_compile_options(cviii_ppu_guard_test_5g2 PRIVATE /UNDEBUG)
else()
    target_compile_options(cviii_ppu_guard_test_5g2 PRIVATE -UNDEBUG)
endif()
add_test(NAME CVIII_5G2_PPU_Qualification COMMAND cviii_ppu_guard_test_5g2)

# 5H-2 pure C11 private indexed-world regression, no ROM required.
add_executable(cviii_native_world_test_5h2
    "${CMAKE_CURRENT_LIST_DIR}/cviii_native_world_5h2.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_trace_5h3.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_gate_5h4.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_native_world_test_5h2.c")
target_compile_features(cviii_native_world_test_5h2 PRIVATE c_std_11)
target_include_directories(cviii_native_world_test_5h2 PRIVATE "${JP_AUDIO_DIR}")
if(MSVC)
    target_compile_options(cviii_native_world_test_5h2 PRIVATE /utf-8 /UNDEBUG)
    target_compile_definitions(cviii_native_world_test_5h2 PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(cviii_native_world_test_5h2 PRIVATE -UNDEBUG)
endif()
add_test(NAME CVIII_5H2_Native_Atlas_Index COMMAND cviii_native_world_test_5h2)

# 5H-3: original-room camera/transition observation regression.
add_executable(cviii_camera_trace_test_5h3
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_trace_5h3.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_gate_5h4.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_trace_test_5h3.c")
target_compile_features(cviii_camera_trace_test_5h3 PRIVATE c_std_11)
if(MSVC)
    target_compile_options(cviii_camera_trace_test_5h3 PRIVATE /UNDEBUG)
    target_compile_definitions(cviii_camera_trace_test_5h3 PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(cviii_camera_trace_test_5h3 PRIVATE -UNDEBUG)
endif()
add_test(NAME CVIII_5H3_Camera_Observation COMMAND cviii_camera_trace_test_5h3)

# 5H-4: ROM-free camera candidate contract; no SDL or engine required.
add_executable(cviii_camera_gate_test_5h4
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_gate_5h4.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_gate_test_5h4.c")
target_compile_features(cviii_camera_gate_test_5h4 PRIVATE c_std_11)
if(MSVC)
    target_compile_options(cviii_camera_gate_test_5h4 PRIVATE /UNDEBUG)
else()
    target_compile_options(cviii_camera_gate_test_5h4 PRIVATE -UNDEBUG)
endif()
add_test(NAME CVIII_5H4_Camera_Candidates COMMAND cviii_camera_gate_test_5h4)

# 5H-5: ROM-free cross-room/fine-scroll parity validation.
add_executable(cviii_world_parity_test_5h5
    "${CMAKE_CURRENT_LIST_DIR}/cviii_world_parity_5h5.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_ppu_audit_5h6.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_track_5h7.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_world_parity_test_5h5.c")
target_compile_features(cviii_world_parity_test_5h5 PRIVATE c_std_11)
target_include_directories(cviii_world_parity_test_5h5 PRIVATE "${JP_AUDIO_DIR}")
if(MSVC)
    target_compile_options(cviii_world_parity_test_5h5 PRIVATE /UNDEBUG /utf-8)
else()
    target_compile_options(cviii_world_parity_test_5h5 PRIVATE -UNDEBUG)
endif()
add_test(NAME CVIII_5H5_Cross_Room_Fine_Scroll COMMAND cviii_world_parity_test_5h5)

# 5H-6 isolated, original-ROM-free per-scanline PPU metadata contract.
add_executable(cviii_ppu_audit_test_5h6
    "${CMAKE_CURRENT_LIST_DIR}/cviii_ppu_audit_5h6.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_track_5h7.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_ppu_audit_test_5h6.c")
target_compile_features(cviii_ppu_audit_test_5h6 PRIVATE c_std_11)
if(MSVC)
  target_compile_options(cviii_ppu_audit_test_5h6 PRIVATE /UNDEBUG /utf-8)
  target_compile_definitions(cviii_ppu_audit_test_5h6 PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(cviii_ppu_audit_test_5h6 PRIVATE -UNDEBUG)
endif()
add_test(NAME CVIII_5H6_Scanline_PPU_Trace COMMAND cviii_ppu_audit_test_5h6)

# 5H-7 ROM-free scroll page tracking regression (assertions enabled in Release).
add_executable(cviii_camera_track_test_5h7
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_track_5h7.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_camera_track_test_5h7.c")
target_compile_features(cviii_camera_track_test_5h7 PRIVATE c_std_11)
if(MSVC)
    target_compile_options(cviii_camera_track_test_5h7 PRIVATE /UNDEBUG)
else()
    target_compile_options(cviii_camera_track_test_5h7 PRIVATE -UNDEBUG)
endif()
add_test(NAME CVIII_5H7_Persistent_Camera_Unwrap COMMAND cviii_camera_track_test_5h7)

# 5H-8: no-ROM capture format regression.
add_executable(cviii_diagnostic_test_5h8
    "${CMAKE_CURRENT_LIST_DIR}/cviii_diagnostic_5h8.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_diagnostic_test_5h8.c")
target_compile_features(cviii_diagnostic_test_5h8 PRIVATE c_std_11)
if(MSVC)
    target_compile_definitions(cviii_diagnostic_test_5h8 PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(cviii_diagnostic_test_5h8 PRIVATE /UNDEBUG)
else()
    target_compile_options(cviii_diagnostic_test_5h8 PRIVATE -UNDEBUG)
endif()
add_test(NAME CVIII_5H8_Evidence_Capture COMMAND cviii_diagnostic_test_5h8)

# Phase 5H-8B: no-ROM room-local camera and margin regression.
add_executable(cviii_room_local_test_5h8b
    "${CMAKE_CURRENT_LIST_DIR}/cviii_room_local_5h8.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_scene_epoch_5h8c.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_dual_scene_5h8c.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_boss_cache_5h8c.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_room_local_test_5h8b.c")
target_include_directories(cviii_room_local_test_5h8b PRIVATE "${JP_AUDIO_DIR}")
target_compile_features(cviii_room_local_test_5h8b PRIVATE c_std_11)
if(MSVC)
    target_compile_options(cviii_room_local_test_5h8b PRIVATE /UNDEBUG /utf-8)
else()
    target_compile_options(cviii_room_local_test_5h8b PRIVATE -UNDEBUG)
endif()
add_test(NAME CVIII_5H8B_Room_Local_Parity COMMAND cviii_room_local_test_5h8b)

# 5H-8C native scene integration; independent synthetic tests.
add_executable(cviii_scene_integration_test_5h8c
    "${CMAKE_CURRENT_LIST_DIR}/cviii_scene_epoch_5h8c.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_dual_scene_5h8c.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_boss_cache_5h8c.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_scene_integration_test_5h8c.c")
target_include_directories(cviii_scene_integration_test_5h8c PRIVATE "${JP_AUDIO_DIR}")
target_compile_features(cviii_scene_integration_test_5h8c PRIVATE c_std_11)
if(MSVC)
    target_compile_options(cviii_scene_integration_test_5h8c PRIVATE /UNDEBUG /utf-8)
else()
    target_compile_options(cviii_scene_integration_test_5h8c PRIVATE -UNDEBUG)
endif()
add_test(NAME CVIII_5H8C_Scene_Integration COMMAND cviii_scene_integration_test_5h8c)

# Phase 5H-8D: ROM-free rolling transition and boss stability tests.
add_executable(cviii_transition_test_5h8d
    "${CMAKE_CURRENT_LIST_DIR}/cviii_transition_test_5h8d.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_transition_5h8d.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_mutable_tiles_5h8e.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_dual_scene_5h8c.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_boss_cache_5h8c.c")
target_include_directories(cviii_transition_test_5h8d PRIVATE "${JP_AUDIO_DIR}")
target_compile_features(cviii_transition_test_5h8d PRIVATE c_std_11)
if(MSVC)
    target_compile_options(cviii_transition_test_5h8d PRIVATE /UNDEBUG /utf-8)
else()
    target_compile_options(cviii_transition_test_5h8d PRIVATE -UNDEBUG)
endif()
add_test(NAME CVIII_5H8D_RollingTransition_BossCache COMMAND cviii_transition_test_5h8d)

# Phase 5H-8E: pure, read-only mutation overlay regression.
add_executable(cviii_mutation_test_5h8e
    "${CMAKE_CURRENT_LIST_DIR}/cviii_mutation_test_5h8e.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_mutable_tiles_5h8e.c")
add_test(NAME CVIII_5H8E_MutableBackground_Observer COMMAND cviii_mutation_test_5h8e)

# Phase 5H-8F: explicit event persistence, no inferred OAM-to-entity mapping.
add_executable(cviii_entity_events_test_5h8f
    "${CMAKE_CURRENT_LIST_DIR}/cviii_entity_events_test_5h8f.c"
    "${CMAKE_CURRENT_LIST_DIR}/cviii_entity_events_5h8f.c")
target_compile_features(cviii_entity_events_test_5h8f PRIVATE c_std_11)
add_test(NAME CVIII_5H8F_Authoritative_EventLedger COMMAND cviii_entity_events_test_5h8f)
