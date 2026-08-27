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

test_inc = include_directories('..', '../launcher')
test_qt_deps = [
  qt_core_dep,
  qt_gui_dep,
  qt_widgets_dep,
  qt_network_dep,
  qt_networkauth_dep,
  qt_concurrent_dep,
  qt_websockets_dep,
  qt_xml_dep,
  qt_opengl_dep,
  qt_multimedia_dep,
  qt_test_dep,
]

foreach test_source : test_sources
  test_name = test_source.split('/')[-1].replace('.cpp', '')
  test_moc_generated = qt.preprocess(
    moc_sources: files(test_source),
    include_directories: [test_inc],
    dependencies: test_qt_deps,
  )
  test_exe = executable(
    test_name,
    files(test_source) + test_moc_generated,
    include_directories: [test_inc],
    link_with: launcher_logic,
    dependencies: test_qt_deps,
  )
  test(test_name, test_exe)
endforeach

curseforge_external_tool_test_source = 'CurseForgeDownloadPageService_test.cpp'
curseforge_external_tool_test_moc = qt.preprocess(
  moc_sources: files(curseforge_external_tool_test_source),
  include_directories: [test_inc],
  dependencies: [qt_core_dep, qt_test_dep],
)
curseforge_external_tool_test = executable(
  'CurseForgeDownloadPageService_test',
  files(
    curseforge_external_tool_test_source,
    '../launcher/modplatform/flame/CurseForgeExternalTool.cpp',
  ) + curseforge_external_tool_test_moc,
  include_directories: [test_inc],
  dependencies: [qt_core_dep, qt_test_dep],
)
test('CurseForgeDownloadPageService_test', curseforge_external_tool_test)
