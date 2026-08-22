test_sources = [
  'FileSystem_test.cpp',
  'GZip_test.cpp',
  'GradleSpecifier_test.cpp',
  'MojangVersionFormat_test.cpp',
  'Library_test.cpp',
  'BMCLAPITest.cpp',
  'BMCLAPICompareTest.cpp',
  'ResourceFolderModel_test.cpp',
  'ResourcePackParse_test.cpp',
  'TexturePackParse_test.cpp',
  'DataPackParse_test.cpp',
  'ShaderPackParse_test.cpp',
  'WorldSaveParse_test.cpp',
  'ParseUtils_test.cpp',
  'Task_test.cpp',
  'INIFile_test.cpp',
  'JavaVersion_test.cpp',
  'Packwiz_test.cpp',
  'PCLPack_test.cpp',
  'Index_test.cpp',
  'Version_test.cpp',
  'MetaComponentParse_test.cpp',
  'CatPack_test.cpp',
  'XmlLogs_test.cpp',
]

foreach test_source : test_sources
  test_name = test_source.split('/')[-1].replace('.cpp', '')
  test_exe = executable(
    test_name,
    files(test_source),
    include_directories: [include_directories('..', '../launcher')],
    link_with: launcher_logic,
    dependencies: [qt_test_dep],
  )
  test(test_name, test_exe)
endforeach
