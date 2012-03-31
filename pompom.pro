TEMPLATE = subdirs
SUBDIRS = src

# remove app bundle
macx {
	CONFIG -= app_bundle
}
