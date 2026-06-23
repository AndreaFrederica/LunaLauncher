# Static source lists for the Meson build.

qtermwidget_moc_headers = [
  'qtermwidget/lib/Emulation.h',
  'qtermwidget/lib/qtermwidget.h',
  'qtermwidget/lib/ScreenWindow.h',
  'qtermwidget/lib/TerminalDisplay.h',
  'qtermwidget/lib/Vt102Emulation.h',
  'qtermwidget/lib/util/Filter.h',
  'qtermwidget/lib/util/HistorySearch.h',
  'qtermwidget/lib/util/SearchBar.h',
]

if is_windows
  qtermwidget_moc_headers += ['qtermwidget/lib/ptyqt/conptyprocess.h']
elif is_linux
  qtermwidget_moc_headers += ['qtermwidget/lib/ptyqt/unixptyprocess.h']
endif

qtermwidget_moc_sources = [
]
