cmake_minimum_required(VERSION 3.21)
project(iblforge LANGUAGES CXX RC)

if(NOT WIN32)
  message(FATAL_ERROR "このプロジェクトは Windows (Direct3D 12) 専用です。")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# ------------------------------------------------------------------
# DirectXTex (HDR読み込み / DDS保存・読み込み)
#   D3D12アップロードは自前実装するので DX11/DX12 サポートは不要
# ------------------------------------------------------------------
set(BUILD_TOOLS  OFF CACHE BOOL "" FORCE)
set(BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
set(BUILD_DX11   OFF CACHE BOOL "" FORCE)
set(BUILD_DX12   OFF CACHE BOOL "" FORCE)
set(BC_USE_OPENMP OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  DirectXTex
  GIT_REPOSITORY https://github.com/microsoft/DirectXTex.git
  GIT_TAG        oct2025
  GIT_SHALLOW    TRUE
)

# ------------------------------------------------------------------
# Dear ImGui (CMakeLists が無いので自前で静的ライブラリ化)
# ------------------------------------------------------------------
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.93.0-docking
  GIT_SHALLOW    TRUE
)

# ------------------------------------------------------------------
# rapidobj (.obj 読み込み, header-only / C++17)
# ------------------------------------------------------------------
FetchContent_Declare(
  rapidobj
  GIT_REPOSITORY https://github.com/guybrush77/rapidobj.git
  GIT_TAG        v1.1
  GIT_SHALLOW    TRUE
)

# ------------------------------------------------------------------
# meshoptimizer v1.3 (メッシュの頂点 / インデックスバッファ最適化。ビルド時ツール meshforge だけが使う)
# ------------------------------------------------------------------
set(MESHOPT_INSTALL OFF CACHE BOOL "" FORCE)           # install 規則は不要
set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(MESHOPT_BUILD_GLTFPACK OFF CACHE BOOL "" FORCE)
set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) # 静的ライブラリ
FetchContent_Declare(
  meshoptimizer
  GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
  GIT_TAG        v1.3
  GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(DirectXTex imgui rapidobj meshoptimizer)

add_library(imgui_lib STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui_lib PUBLIC d3d12 dxgi d3dcompiler dwmapi)
target_compile_definitions(imgui_lib PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)

# ------------------------------------------------------------------
# 表示用モデル (alecjacobson/common-3d-test-models の .obj)
#   Stanford Bunny / Armadillo / Happy Buddha / Lucy (各 2〜5MB)。
#   コミットを固定した URL + SHA256 検証でダウンロードする (configure 時。検証済みなら再ダウンロードしない)。
#   さらにビルド時に meshforge (下記) で、meshoptimizer による頂点/インデックスの最適化を行ったうえでバイナリ (.mesh) へ変換し、exe の隣の models/ に置く。
#   実行時の exe は .mesh を読むだけ (.obj のパースや rapidobj は実行時には不要)。
#   失敗してもビルドは続行し、実行時は UI でそのモデルが選べなくなるだけ (球体は内蔵)。
# ------------------------------------------------------------------
set(MODEL_COMMIT "8a4f8642acaf43f9cd7b67858a1502e1055ef202")
# "<.obj ファイル名>|<SHA256>|<pitch(度)>|<yaw(度)>"。
#   pitch / yaw: 「Y が上で、正面が初期カメラ (-Z 側) に向く」ようにする向きの補正 (MeshLoader.h 参照)。変換時に焼き込まれる。
#   出力は <名前>.mesh。gfx/MeshLibrary.cpp の kModels のファイル名と一致させること。
set(MODEL_LIST
  "stanford-bunny.obj|1eb35d1e21ce99e5ce911353b6be278990713448dd9e8f5c9387f9de39b32205|0|-90"
  "armadillo.obj|d2fd040334c12b16804fbcb9962391232cc24708244617f931baaa871210563e|0|180"
  "happy.obj|8ce5a4a7161efd97dabc8eeb281824fac1b17c9ba2cb0c2c6f350cd05982058c|0|0"
  "lucy.obj|ea54611a3466bc903a107fd825409344562b8c4a8f191bbdc88ce380bebd0911|90|180"
)

# .obj -> .mesh 変換ツール (ビルド時に実行される。rapidobj と meshoptimizer はこのツールだけが使う)
add_executable(meshforge
  tools/meshforge.cpp
  tools/mesh_optimize.cpp
  tools/mesh_optimize.h
  src/mesh/MeshFile.cpp
  src/mesh/MeshFile.h
  src/mesh/MeshLoader.cpp
  src/mesh/MeshLoader.h
)
target_include_directories(meshforge PRIVATE src tools)
target_link_libraries(meshforge PRIVATE rapidobj::rapidobj meshoptimizer)
target_compile_definitions(meshforge PRIVATE _CRT_SECURE_NO_WARNINGS)
if(MSVC)
  # ソースは UTF-8 (BOM なし) で日本語コメントを含む。/utf-8 が無いと日本語環境の MSVC は cp932 として読み、
  # コメント末尾の文字が改行を巻き込んで次の行 (閉じ括弧や #include) を飲み込み、namespace が閉じずに大量のエラーになる。
  target_compile_options(meshforge PRIVATE /utf-8 /W3)
endif()
set(MODEL_MESH_OUTPUTS "")  # 変換して作る .mesh のパス (後で exe の隣へコピーする)
# file(DOWNLOAD ... EXPECTED_HASH) はハッシュ不一致だと configure が即エラーになり、しかも壊れたファイルが残って
# 次回は「存在する」と見なされてしまう。そのため自前で検証し、不一致なら削除して警告にとどめる。
foreach(entry IN LISTS MODEL_LIST)
  string(REPLACE "|" ";" entry_parts "${entry}")
  list(GET entry_parts 0 model_name)
  list(GET entry_parts 1 model_sha256)
  list(GET entry_parts 2 model_pitch)
  list(GET entry_parts 3 model_yaw)
  set(model_url  "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/${MODEL_COMMIT}/data/${model_name}")
  set(model_file "${CMAKE_BINARY_DIR}/models/${model_name}")
  set(model_ok FALSE)
  if(EXISTS "${model_file}")
    file(SHA256 "${model_file}" model_actual)
    if(model_actual STREQUAL model_sha256)
      set(model_ok TRUE)
    else()
      file(REMOVE "${model_file}")  # 途中で切れた/改変されたファイルは捨てて取り直す
    endif()
  endif()
  if(NOT model_ok)
    message(STATUS "Downloading ${model_name}: ${model_url}")
    file(DOWNLOAD "${model_url}" "${model_file}" TLS_VERIFY ON STATUS model_status)
    list(GET model_status 0 model_status_code)
    list(GET model_status 1 model_status_msg)
    if(model_status_code EQUAL 0)
      file(SHA256 "${model_file}" model_actual)
      if(model_actual STREQUAL model_sha256)
        set(model_ok TRUE)
      else()
        set(model_status_msg "SHA256 mismatch: ${model_actual}")
      endif()
    endif()
    if(NOT model_ok)
      file(REMOVE "${model_file}")
      message(WARNING "${model_name} のダウンロードに失敗しました (${model_status_msg})。"
                      "UI の Shape でこのモデルが選べなくなります。再 configure でやり直せます。")
    endif()
  endif()
  if(model_ok)
    # .obj -> .mesh (.obj か meshforge が更新されたときだけ再変換される)
    get_filename_component(model_base "${model_name}" NAME_WE)
    set(mesh_file "${CMAKE_BINARY_DIR}/models/${model_base}.mesh")
    add_custom_command(
      OUTPUT "${mesh_file}"
      # meshforge.exe を直接実行せず cmake -E env 経由にする: Ninja + cmd.exe では、コマンド先頭のパスに ( ) を含むと
      # (例: C:\Users\me\Downloads\iblforge(1)\...) cmd が "(" でパスを切ってしまい実行できない。cmake.exe のパスは通常 ( ) を含まない。
      COMMAND ${CMAKE_COMMAND} -E env "$<TARGET_FILE:meshforge>" "${model_file}" "${mesh_file}" --pitch ${model_pitch} --yaw ${model_yaw}
      DEPENDS meshforge "${model_file}"
      COMMENT "Baking ${model_name} -> ${model_base}.mesh"
      VERBATIM)
    list(APPEND MODEL_MESH_OUTPUTS "${mesh_file}")
  endif()
endforeach()

# ------------------------------------------------------------------
# 本体
# ------------------------------------------------------------------
# ソースはディレクトリ (責務) ごとに分けてある:
#   app/ ウィンドウ・イベントループ・アプリ状態  ui/ ImGui のパネル  gfx/ D3D12 描画  bake/ ベイク (CPU/GPU/入出力)  mesh/ モデル形式
file(GLOB_RECURSE IBLFORGE_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)
# meshforge 専用の rapidobj 依存 (MeshLoader.cpp) は本体 exe に含めない
list(FILTER IBLFORGE_SOURCES EXCLUDE REGEX "/src/mesh/MeshLoader\\.cpp$")
add_executable(iblforge WIN32
  ${IBLFORGE_SOURCES}
  resources/app.rc
  resources/resource.h
  shaders/pbr.hlsl
  shaders/ibl_bake.hlsl
  shaders/taa.hlsl
)
target_include_directories(iblforge PRIVATE src resources)
target_link_libraries(iblforge PRIVATE
  DirectXTex imgui_lib d3d12 dxgi d3dcompiler comdlg32 shell32 ole32 user32 dwmapi advapi32
)
target_compile_definitions(iblforge PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_source_files_properties(shaders/pbr.hlsl shaders/ibl_bake.hlsl shaders/taa.hlsl PROPERTIES HEADER_FILE_ONLY TRUE)

if(MSVC)
  target_compile_options(iblforge PRIVATE /utf-8 /W3 /MP)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT iblforge)
  set_target_properties(iblforge PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# シェーダーは実行時に D3DCompileFromFile でコンパイルする。exe の隣へコピー。
add_custom_command(TARGET iblforge POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:iblforge>/shaders
)

# 変換済みモデル (.mesh) を exe の隣の models/ へコピー (Renderer は ExeDir()/models/<name>.mesh を読む)
if(MODEL_MESH_OUTPUTS)
  add_custom_target(iblforge_models DEPENDS ${MODEL_MESH_OUTPUTS})
  add_dependencies(iblforge iblforge_models)
  add_custom_command(TARGET iblforge POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:iblforge>/models
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MODEL_MESH_OUTPUTS} $<TARGET_FILE_DIR:iblforge>/models
  )
endif()
