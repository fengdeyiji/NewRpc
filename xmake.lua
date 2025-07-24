set_project("new_rpc")
set_version("1.0.0")
-- 设置使用 clang 编译器
set_toolset("cc", "clang")
set_toolset("cxx", "clang++")
set_toolset("ld", "clang++")
set_toolset("sh", "clang++")
-- 基于 LLVM 基本路径设置 include 路径和 lib 路径
-- llvm_prefix = "/home/admin/project/LLVM"
-- target_triplet = nil
-- if is_arch("x86_64") then
--     target_triplet = "x86_64-unknown-linux-gnu"
-- elseif is_arch("aarch64") then
--     target_triplet = "aarch64-unknown-linux-gnu"
-- else
--     target_triplet = "x86_64-unknown-linux-gnu"
-- end
-- add_includedirs("src",
--                 llvm_prefix .. "/include/c++/v1",
--                 llvm_prefix .. "/include/" .. target_triplet .. "/c++/v1",
--                 "/usr/local/include")
-- add_linkdirs(llvm_prefix .. "/lib/" .. target_triplet,
--              "/usr/local/lib")
-- 设置 C++ 标准库为 libc++
-- add_cxxflags("-std=c++23", "-stdlib=libc++")
-- add_ldflags("-stdlib=libc++", "-nostdlib++", "-lc++", "-lc++abi")
-- add_ldflags("-static", "-lpthread")

add_cxxflags("-std=c++23")
add_includedirs("src")
-- for spdlog
add_includedirs("/home/admin/project/spdlog/include")
-- for boost
add_includedirs("/home/admin/.local/include/")
add_linkdirs("/home/admin/.local/lib/")
-- add_ldflags("-static")
add_ldflags("-lpthread")

add_files("demo/example_rpc.cpp")
add_files("src/log/*.cpp")
add_files("src/error_define/*.cpp")
add_files("src/coroutine_framework/*.cpp")
add_files("src/coroutine_framework/net_module/*.cpp")
add_files("src/coroutine_framework/time_module/*.cpp")

-- 模式相关配置
if is_mode("debug") then
  add_defines("DEBUG")
  add_cxflags("-O0", "-g3", "-march=armv8-a")
  set_targetdir("build_debug")
elseif is_mode("release") then
  add_defines("NDEBUG")
  add_cxflags("-O3")
  set_targetdir("build_release")
end

target("main")
  set_kind("binary")
  add_files("src/main.cpp")

target("demo_1_sleep2")
  set_kind("binary")
  add_files("demo/demo_1_sleep2.cpp")

target("demo_2_sleep2_concurrent")
  set_kind("binary")
  add_files("demo/demo_2_sleep2_concurrent.cpp")

target("demo_3_sleep_millions")
  set_kind("binary")
  add_files("demo/demo_3_sleep_millions.cpp")

target("demo_4_rpc_call")
  set_kind("binary")
  add_files("demo/demo_4_rpc_call.cpp")

target("demo_5_multi_rpc_call")
  set_kind("binary")
  add_files("demo/demo_5_multi_rpc_call.cpp")

-- -- 创建测试项目
target("unittests")
  add_links("boost_unit_test_framework")  -- 显式链接测试框架
  set_kind("binary")
  add_files("unittests/*.cpp")
  -- 添加测试执行目标
  add_tests("test_stringification", { runargs = { "--run_test=test_stringification" } })
  -- 自动扫描 tests 目录下的测试文件
  for _, file in ipairs(os.files("tests/test_*.cpp")) do
    local testname = path.basename(file, true)  -- 提取文件名（不含扩展名）
    -- 自动为每个测试文件生成测试用例
    add_tests(testname, { runargs = {"--run_test=" .. testname} })
  end

  -- 添加全部测试
  add_tests("test_all", { 
    runargs = { 
      "--log_level=test_suite", 
      "--output_format=XML",
      "--report_level=detailed" 
    }
  })
  -- 模式相关配置
  if is_mode("debug") then
    add_defines("DEBUG")
    add_cxflags("-O0", "-g3")
    set_targetdir("build_debug")
  elseif is_mode("release") then
    add_defines("NDEBUG")
    add_cxflags("-O3")
    set_targetdir("build_release")
  end
  -- -- 启用覆盖率（如果支持）
  -- if is_mode("debug") and is_plat("linux", "macosx") then
  --   add_cxflags("-fprofile-arcs -ftest-coverage")
  --   -- add_ldflags("--coverage")
  -- end