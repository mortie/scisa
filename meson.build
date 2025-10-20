project(
  'scisa', 'cpp',
  default_options: [
    'cpp_std=c++20',
    'warning_level=3',
  ],
)

libscisavm = declare_dependency(
  include_directories: 'scisavm/include',
  link_with: library('scisavm',
    'scisavm/src/scisavm.cc',
    install: true,
    include_directories: ['scisavm/include']
  ),
)
install_headers(
  'scisavm/include/scisavm.h',
  subdir: 'scisa',
)


libscisasm = declare_dependency(
  include_directories: 'scisasm/include',
  link_with: library('scisasm',
    'scisasm/src/scisasm.cc',
    install: true,
    include_directories: ['scisasm/include']
  ),
)
install_headers(
  'scisasm/include/scisasm.h',
  subdir: 'scisa',
)

scisa = executable(
  'scisa',
  'bin/scisa.cc',
  dependencies: [
    libscisavm,
    libscisasm,
  ],
)
