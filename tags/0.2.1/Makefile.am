DEBUG = -g -O0
INCLUDES = \
	-DVERSION=\"".3"\"  \
	-DPKGDATADIR=\""$(pkgdatadir)"\"  \
	-DBINDIR=\""$(bindir)"\"  	\
	-DSYSCONFDIR=\""$(sysconfdir)"\"   \
	$(GTK_CFLAGS) \
	$(OPENCV_CFLAGS) \
	$(GST_CFLAGS) 


bin_PROGRAMS = gtk-facemanager \
	       gtk-facetracker \
	       svm-predict \
	       svm-train \
	       pfatrainer

pfatrainer_SOURCES =	./libsvm/svm.h \
			./libsvm/svm-scale.c \
                        ./libsvm/svm.cpp \
			 ./libsvm/libsvm-train.c \
			trainer.c

svm_predict_SOURCES = ./libsvm/svm-predict.c \
		      ./libsvm/svm.h \
		      ./libsvm/svm.cpp

svm_train_SOURCES = 	./libsvm/svm-train.c \
			./libsvm/svm.h \
			./libsvm/svm.cpp

gtk_facemanager_SOURCES = gtk-facemanagerU.c \
			featureDctMod2.c \
			featureLBPHist.c \
			gtk-facemanagerU.h \
			libfacedetect.la

install-exec-hook:
		   chown root $(DESTDIR)/$(bindir)/pfatrainer
		   chmod u+s $(DESTDIR)/$(bindir)/pfatrainer
		   chown root $(DESTDIR)/$(bindir)/svm-predict
		   chmod u+s $(DESTDIR)/$(bindir)/svm-predict

gtk_facemanager_LDADD = \
	$(GTK_LIBS) \
	$(OPENCV_LIBS) \
	$(GST_LIBS) \
	libfacedetect.la

gtk_facemanager_LDFLAGS = -export-dynamic
gtk_facetracker_SOURCES = gtk-faceauthenticate.c
gtk_facetracker_LDADD = \
			$(GTK_LIBS) \
			$(OPENCV_LIBS) \
			libfacedetect.la

gtk_facetracker_LDFLAGS = -export-dynamic
extradir=$(prefix)/lib/
lib_LTLIBRARIES = libfaceauthenticate.la
extra_LTLIBRARIES = libfacedetect.la 
libfaceauthenticate_la_SOURCES=libfaceauthenticate.c \
				featureDctMod2.c \
				libfaceauthenticate.h \
				featureLBPHist.c \
				parseSvmPrediction.c
libfaceauthenticate_la_CFLAGS = $(AM_CFLAGS)
libfacedetect_la_SOURCES=libfacedetect.c \
			 libfacedetect.h
libfacedetect_la_CFLAGS = $(AM_CFLAGS)
pammod_PROGRAMS = pam_face_authenticate.so
pammoddir=/lib/security

pam_face_authenticate_so_SOURCES = \
	pam_face_authentication.c \
	pam_face.h \
	featureDctMod2.c \
	featureLBPHist.c \
	parseSvmPrediction.c \
	./libsvm/svm-scale.c \
	pam_face_defines.h

pam_face_authenticate_so_CFLAGS = -fPIC -lpthread
pam_face_authenticate_so_LDFLAGS = -Xcompiler -shared 
pam_face_authenticate_so_LDADD = \
                        $(PAM_LIBS) \
                        $(OPENCV_LIBS) \
			$(GST_LIBS) \
			libfacedetect.la \
                        libfaceauthenticate.la
			
extra1dir=$(sysconfdir)/pam-face-authentication/
extra2dir=$(sysconfdir)/pam-face-authentication/dummy/
extra1_DATA = xauth.key display.key prediction db.lst
extra2_DATA = featuresAverage featuresDistance featuresDCT featuresLBP
uidir=$(pkgdatadir)
ui_DATA = \
gtk-faceauthenticate.xml \
logo.png \
gtk-facemanagerU.xml \
haarcascade_eye.xml \
haarcascade_eye_tree_eyeglasses.xml \
haarcascade_nose.xml \
haarcascade.xml

EXTRA_DIST=$(ui_DATA) $(extra1_DATA) $(extra2_DATA)

