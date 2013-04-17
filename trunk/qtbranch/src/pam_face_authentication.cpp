/*
    Copyright (C) 2008-2009
     Rohan Anil (rohan.anil@gmail.com)
     Alex Lau ( avengermojo@gmail.com)

    Rewritten
    Google Summer of Code Program 2009
    Mentoring Organization: Pardus
    Mentor: Onur Kucuk

    Google Summer of Code Program 2008
    Mentoring Organization: openSUSE

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#define DEFAULT_USER "nobody"
#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
#define PAM_SM_SESSION
#define PAM_SM_PASSWORD

// PAM and system headers
#include <security/pam_modules.h>
#include <security/_pam_macros.h>
#include <security/_pam_types.h>
#include <pwd.h> /* getpwdid */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <iostream>

// OpenCV headers
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/imgproc/types_c.h"
#include "opencv2/imgproc/imgproc_c.h"

// App related headers
#include <stdio.h>
#include <libintl.h> // gettext()
#include <syslog.h>
#include <X11/Xutil.h> // XDestroyImage()
#include "pam_face_authentication.h"
#include "pam_face_defines.h"
#include "webcamImagePaint.h"
#include "opencvWebcam.h"
#include "detector.h"
#include "verifier.h"
// Check for encryption
#include <dirent.h>
#include <string>

//------------------------------------------------------------------------------
bool msgPipeLiner(char *msg)
{
    if(prevmsg != 0 && strcmp(prevmsg, msg) == 0) return 0;
    if(prevmsg != 0) free(prevmsg);
    
    prevmsg = (char *)calloc(strlen(msg)+1, sizeof(char));
    strcpy(prevmsg, msg);
    
    return 1;
}

//------------------------------------------------------------------------------
static int send_msg(pam_handle_t* pamh, char* msg, int type)
{
    const struct pam_conv* pc;
    struct pam_message mymsg;
    struct pam_response* resp;
    
    if(boolMessages == false) return 0;
    if(msgPipeLiner(msg) == false) return 0;

    if(type == 1) mymsg.msg_style = PAM_ERROR_MSG;  // print error msg
    else mymsg.msg_style = PAM_TEXT_INFO;  // normal output
    mymsg.msg = msg;
    const struct pam_message* msgp = &mymsg;

    int r = pam_get_item(pamh, PAM_CONV, (const void **)&pc);
    if(r != PAM_SUCCESS) return -1;

    if(!pc || !pc->conv) return -1;

    return pc->conv(1, &msgp, &resp, pc->appdata_ptr);
}

//------------------------------------------------------------------------------
void resetFlags()
{
    *commAuth = 0;
}

//------------------------------------------------------------------------------
bool ipcStart()
{
    /*   IPC   */
    ipckey = IPC_KEY_IMAGE;
    shmid = shmget(ipckey, IMAGE_SIZE, IPC_CREAT | 0666);
    shared = (char *)shmat(shmid, NULL, 0);
	if ( !shared) {
		syslog( LOG_CRIT, "No attach of shm %lx - %m", ipckey);
	}

    ipckeyCommAuth = IPC_KEY_STATUS;
    shmidCommAuth = shmget(ipckeyCommAuth, sizeof(int), IPC_CREAT | 0666);
    commAuth = (int *)shmat(shmidCommAuth, NULL, 0);
	if ( !commAuth) {
		syslog( LOG_CRIT, "No attach of shm %lx - %m", ipckeyCommAuth);
	}

    *commAuth = 0;
	return ( shared != NULL && commAuth != NULL);
    /*   IPC END  */
}

//------------------------------------------------------------------------------
void writeImageToMemory(IplImage* img, char* shared)
{
	if ( !img || !shared) return;
    for(int n = 0; n < img->height; n++)
    {
        for(int m = 0; m < img->width; m++)
        {
            CvScalar s = cvGet2D(img, n, m);
            int val3 = (uchar)s.val[2];
            int val2 = (uchar)s.val[1];
            int val1 = (uchar)s.val[0];

            *(shared + m*3 + 2+ n*IMAGE_WIDTH*3) = val3;
            *(shared + m*3 + 1+ n*IMAGE_WIDTH*3) = val2;
            *(shared + m*3 + 0+ n*IMAGE_WIDTH*3) = val1;
        }
    }
}

//------------------------------------------------------------------------------
XImage* CreateTrueColorImage(Display* display, Visual* visual, 
  char* image, int width, int height, IplImage* img)
{
    int max = (img->width > img->height) ? img->width:img->height;
    int wh = (img->width > img->height) ? 1 : 0;

    char* image32=(char *)malloc(width*height*4);
    char* p = image32;

    for(int j = 0; j < height; j++)
    {
        for(int i = 0; i < width; i++)
        {
            if((j < img->height) && (i < img->width))
            {
                CvScalar s = cvGet2D(img, j, i);
                int val3 = (uchar)s.val[2];
                int val2 = (uchar)s.val[1];
                int val1 = (uchar)s.val[0];

                *p++ = val1; // blue
                *p++ = val2; // green
                *p++ = val3; // red
            }
            else
            {
                *p++ = 0; // blue
                *p++ = 0; // green
                *p++ = 0; // red
            }
        p++;
        }
    }
    
    return XCreateImage(display, visual, 24, ZPixmap, 0, 
      (char *)image32, width, height, 32, 0);
}

//------------------------------------------------------------------------------
void processEvent(Display* display, Window window, 
  int width, int height, IplImage* img, int s)
{
    int xoffset = (DisplayWidth(display, s) - img->width)/2;
    int yoffset = (DisplayHeight(display, s) - img->height)/2;
    // XMoveWindow(display, window, xoffset, yoffset);
    
    Visual* visual = DefaultVisual(display, 0);
    XImage* ximage = CreateTrueColorImage(display, visual, 0, width, height, img);
    XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, 0, 0, width, height);
    XDestroyImage(ximage);
}

//------------------------------------------------------------------------------
// First we check whether the user's homedir is empty.
// If yes, we assume the homedir to be encrypted and not mounted.
// If no, we look for a file "README.txt" (like found on Ubuntu 9.10) and
// check whether this file is a symlink to a path containing "ecryptfs-utils"
// If yes, we assume the homedir to be encrypted and not mounted.
// Returns true if the homedir seems to be encrypted and not mounted.
bool isHomeEncrypted(const char* homedir)
{
    char encFile[FILENAME_MAX];
    char encLink[FILENAME_MAX];
    int linkNameSize;
    bool dirEmpty = true;
    DIR* home;
    struct dirent* entry;
    struct stat fileStat;

    home = opendir(homedir);
    if(home != NULL)
    {
        while((entry = readdir(home)) != NULL)
        {
            if(std::string(entry->d_name).compare(".") == 0) continue;
            if(std::string(entry->d_name).compare("..") == 0) continue;
            //directory is not empty
            dirEmpty = false;
            break;
        }
        closedir(home);
    }
    else return true; // couldn't open homedir
    
    if(dirEmpty) return true; // empty so probably encrypted
    
    // Looking for a file "README.txt"
    sprintf(encFile, "%s/README.txt", homedir);
    
    if(stat(encFile,&fileStat) < 0) return false; // file seems not to exist
    if(S_ISLNK(fileStat.st_mode)) return false; // file is not a symlink
    if((linkNameSize=readlink(encFile,encLink,FILENAME_MAX)) < 0) return false; // could not get the link filename
    
    encLink[linkNameSize] = '\0';         // terminate the filename
    std::string s = encLink;
    if(s.find("ecryptfs-utils") == std::string::npos)
      return false; // sequence "ecryptfs-utils" not found in filename
    
    return true;
}
void bye() {
    fprintf( stderr, "Exited pam_face_authentication\n");
}
//------------------------------------------------------------------------------
PAM_EXTERN
int pam_sm_authenticate(pam_handle_t* pamh, int flags, int argc, const char** argv)
{
	setlocale(LC_ALL, "");
	bindtextdomain("pam_face_authentication", PKGDATADIR "/locale");
	textdomain("pam_face_authentication");
	openlog("pam_face_authentication", LOG_PID, LOG_AUTHPRIV);

	Display* displayScreen;
	FILE* xlock = NULL;
	Window window;
	Atom wmDeleteMessage;
	struct passwd* userStruct;

	int k = 0, s, retval, retValMsg, procnumber = 0, length = 0, enableX = 0;
	int width = IMAGE_WIDTH, height = IMAGE_HEIGHT;
	const char* host = NULL;
	const char* pamtty = NULL;
	const char* user = NULL;
	const char* user_request = NULL;
	const char* error = NULL;
	char* username = NULL;

	const char* display = getenv("DISPLAY");
	const char* displayOrig = getenv("DISPLAY");
	char* xauthpath = getenv("XAUTHORITY");
	char* xauthpathOrig = getenv("XAUTHORITY");
	char X_lock[300], cmdline[300];
	char local_hostname[50];

	// The following lines make sure that the program quits if it's called remotely
    atexit(bye);
	gethostname(local_hostname, 49);
	retval = pam_get_item(pamh, PAM_RHOST, (const void**)&host);
	if(host != NULL && host != "localhost" && strcmp(local_hostname, host)) 
		return retval;

	// Fetch the current user name
	retval = pam_get_user(pamh, &user, NULL);
	if(retval != PAM_SUCCESS)
	{
		syslog(LOG_ERR, "pam_get_user returned error: %s", pam_strerror(pamh,retval));
		return retval;
	}
	if (user == NULL || *user == '\0')
	{
		syslog(LOG_ALERT, "username not known");
		pam_set_item(pamh, PAM_USER, (const void *) DEFAULT_USER);
		send_msg(pamh, (char*)"Username not set.", 1);
		return PAM_AUTHINFO_UNAVAIL;
	}

	// removed Xauth stuff Not Needed for KDM or GDM
	if (!ipcStart()) {
		return PAM_AUTHINFO_UNAVAIL;
	}
	resetFlags();

	username = (char *)calloc(strlen(user)+1, sizeof(char));
	strcpy(username,user);

	userStruct = getpwnam(username);
	uid_t userID = userStruct->pw_uid;
	verifier* newVerifier = new verifier(userID);

	pam_get_item(pamh, PAM_TTY, (const void **)(const void*)&pamtty);
	if(pamtty != NULL && strlen(pamtty) > 0 && pamtty[0] == ':' && display == NULL)
	{
		display = pamtty;
		if(displayOrig == NULL) setenv("DISPLAY", display, -1);
	}

	// Check if the user's homedir is encrypted
	if(isHomeEncrypted(userStruct->pw_dir)) {
		syslog(LOG_ALERT, "user home encrypted");
		return PAM_AUTHINFO_UNAVAIL;
	}

	while(k < argc)
	{
		if(strcmp(argv[k], "gdmlegacy") == 0)
		{
			char str[50];
			char* word = NULL;
			char* word1 = NULL;

			snprintf(X_lock, 300, "/tmp/.X%s-lock", strtok((char*)&display[1], "."));

			xlock = fopen(X_lock, "r");
			fgets(cmdline, 300, xlock);
			fclose(xlock);

			word1 = strtok(cmdline,"  \n");
			snprintf(X_lock, 300,  "/proc/%s/cmdline", word1);
			xlock = fopen(X_lock, "r");
			fgets(X_lock , 300, xlock);
			fclose(xlock);

			for(int j = 0; j < 300; j++)
			{
				if (X_lock[j]=='\0') X_lock[j]=' ';
			}

			for(word = strtok(X_lock, " "); word != NULL; word = strtok(NULL," "))
			{
				if(strcmp(word, "-auth") == 0)
				{
					xauthpath = strtok(NULL, " ");
					break;
				}
			}
			if(file_exists(xauthpath) == 1) setenv("XAUTHORITY", xauthpath, -1);
		}

		if((strcmp(argv[k], "enableX") == 0) || (strcmp(argv[k], "enablex") == 0))
		{
			pam_get_item(pamh, PAM_RUSER, (const void **)(const void*)&user_request);

			if(user_request != NULL)
			{
				struct passwd* pw = getpwnam(user_request);
				if(pw != NULL && xauthpathOrig == NULL)
				{  
					char xauthPathString[300];
					snprintf(xauthPathString, 300, "%s/.Xauthority", pw->pw_dir);
					setenv("XAUTHORITY",xauthPathString,-1);
				}
			}

			displayScreen = XOpenDisplay(NULL);
			if(displayScreen != NULL)
			{
				s = DefaultScreen(displayScreen);
				int xoffset = (DisplayWidth(displayScreen,s) - width)/2;
				int yoffset = (DisplayHeight(displayScreen,s) - height)/2;

				// printf("%d  %d\n",xoffset ,yoffset);

				window = XCreateSimpleWindow(displayScreen, 
						RootWindow(displayScreen, s), xoffset, xoffset, width, height, 1, 0, 0);
				//XSelectInput(displayScreen, window, ButtonPressMask|ExposureMask);
				wmDeleteMessage = XInternAtom(displayScreen, "WM_DELETE_WINDOW", false);
				XSetWMProtocols(displayScreen, window, &wmDeleteMessage, 1);
				XMapWindow(displayScreen, window);
				XMoveWindow(displayScreen, window, xoffset, yoffset);

				enableX = 1;
			}
		}

		if(strcmp(argv[k], "disable-messages") == 0) boolMessages = false;

		k++;
	}

	opencvWebcam webcam;
	detector newDetector;
	static webcamImagePaint newWebcamImagePaint;

	/* Clear Shared Memory */
	IplImage* zeroFrame = cvCreateImage(cvSize(IMAGE_WIDTH, IMAGE_HEIGHT), IPL_DEPTH_8U, 3);
	cvZero(zeroFrame);
	writeImageToMemory(zeroFrame, shared);


	std::cerr << "Starting the camera, pid ("<< getpid() << ")" << std::endl;
	if(webcam.startCamera() == 0)
	{
		//Awesome Graphic Could be put to shared memory over here [TODO]
		send_msg(pamh, gettext("Unable to get hold of your webcam. Please check if it is plugged in."), 1);
		syslog(LOG_CRIT, "Cannot open camera");
		return PAM_AUTHINFO_UNAVAIL;
	}

	/* New Logic, run it for some X amount of Seconds and Reply Not Allowed */

	// This line might be necessary for GDM , will fix later, right now KDM
	// system(QT_FACE_AUTH);
	double t1 = (double)cvGetTickCount();
	double t2 = 0;
	double t3 = 0;
	bool run_loop = true;
	int timeout = 25000; // Login timeout value

	*commAuth = STARTED;

	// Don't Gettext this because kgreet_plugin relies on this :)
	send_msg(pamh, (char*)"Face Verification Pluggable Authentication Module Started");
	int val = newVerifier->verifyFace(zeroFrame);
	if(val == -1)
	{
		send_msg(pamh, gettext("Biometrics model has not been generated for the user. \
					Use qt-facetrainer to create the model."));
		run_loop = false;
		webcam.stopCamera();
		return PAM_USER_UNKNOWN;
	}

	//send_msg(pamh, "Commencing Face Verification.");

	CvFileStorage* fileStorage = cvOpenFileStorage(PKGDATADIR "/config.xml", 0, CV_STORAGE_READ);
	if(fileStorage)
	{
		timeout = cvReadIntByName(fileStorage, 0, "TIME_OUT", timeout);
		cvReleaseFileStorage(&fileStorage);
	}

	int consecutive = 0;
	while(run_loop == true && t2 < timeout)
	{
		t2 = (double)cvGetTickCount() - t1;
		t2 = t2 / ((double)cvGetTickFrequency()*1000.0);

		IplImage* queryImage = webcam.queryFrame();

		if(queryImage != 0)
		{
			newDetector.runDetector(queryImage);

			if(sqrt(pow(newDetector.eyesInformation.LE.x - newDetector.eyesInformation.RE.x, 2) 
						+ (pow(newDetector.eyesInformation.LE.y-newDetector.eyesInformation.RE.y, 2))) > 28  
					&& sqrt(pow(newDetector.eyesInformation.LE.x-newDetector.eyesInformation.RE.x, 2) 
						+ (pow(newDetector.eyesInformation.LE.y-newDetector.eyesInformation.RE.y, 2))) < 120)
			{
				double yvalue = newDetector.eyesInformation.RE.y - newDetector.eyesInformation.LE.y;
				double xvalue = newDetector.eyesInformation.RE.x - newDetector.eyesInformation.LE.x;
				double ang = atan(yvalue / xvalue) * (180 / CV_PI);

				if(pow(ang, 2) < 200)
				{
					IplImage* im = newDetector.clipFace(queryImage);
					if(im != 0)
					{
						consecutive++;
						send_msg(pamh, gettext("Verifying Face ..."));
						int val = newVerifier->verifyFace(im);
						if(val > 0)
						{
							*commAuth = STOPPED;
							// cvSaveImage("/home/rohan/new1.jpg",newDetector.clipFace(queryImage));
							send_msg(pamh, gettext("Verification successful."));

							if(enableX == 1)
							{
								XDestroyWindow(displayScreen, window);
								XCloseDisplay(displayScreen);
							}

							writeImageToMemory(zeroFrame, shared);
							webcam.stopCamera();

                            /*
							t2 = (double)cvGetTickCount();

							while(t3 < 1300) t3 = (double)cvGetTickCount() - t2;
                            */

							std::cerr << "Closing the camera" << std::endl;
							webcam.stopCamera();
							return PAM_SUCCESS;
						}
						cvReleaseImage(&im);
					} else {
						consecutive = 0;
					}
				}
				else {
					send_msg(pamh, gettext("Align your face."));
					consecutive = 0;
				}

				newWebcamImagePaint.paintCyclops(queryImage, 
						newDetector.eyesInformation.LE, newDetector.eyesInformation.RE);
				newWebcamImagePaint.paintEllipse(queryImage, 
						newDetector.eyesInformation.LE, newDetector.eyesInformation.RE);

				//  cvLine(queryImage, newDetector.eyesInformation.LE, 
				//    newDetector.eyesInformation.RE, cvScalar(0,255,0), 4);
			}
			else {
				send_msg(pamh,gettext("Keep proper distance with the camera."));
				consecutive = 0;
			}


			IplImage *scaledImage = cvCreateImage(cvSize(IMAGE_WIDTH,
						IMAGE_HEIGHT), 8, 3);
			cvResize(queryImage, scaledImage, CV_INTER_LINEAR);

			if(enableX == 1) 
			{
				processEvent(displayScreen, window, width, height, scaledImage, s);
				while(XPending(displayScreen))
				{
					XEvent event;
					XNextEvent(displayScreen, &event);
					if(event.type == ClientMessage && event.xclient.data.l[0] == wmDeleteMessage)
					{
						send_msg(pamh, (char*)"Shutting down now!");
						XDestroyWindow(displayScreen, window);
						XCloseDisplay(displayScreen);
						webcam.stopCamera();
						return PAM_AUTHINFO_UNAVAIL;
					}
				}
			}

			writeImageToMemory(scaledImage, shared);
			cvReleaseImage(&scaledImage);
		}
		else send_msg(pamh, gettext("Unable query image from your webcam."));
		if ( consecutive > 19)
			run_loop=false;
	}

	writeImageToMemory(zeroFrame, shared);

	send_msg(pamh, gettext("Giving Up Face Authentication. Try Again."), 1);

	if(enableX == 1)
	{
		XDestroyWindow(displayScreen,window);
		XCloseDisplay(displayScreen);
	}

	*commAuth = STOPPED;
	std::cerr << "Closing the camera, giving up ("<< getpid() << ")" << std::endl;
	webcam.stopCamera();

	syslog(LOG_NOTICE, "Timeout or failure");
	return PAM_AUTH_ERR;
}

//------------------------------------------------------------------------------
PAM_EXTERN
int pam_sm_setcred(pam_handle_t* pamh, int flags, int argc, const char** argv)
{
    return PAM_SUCCESS;
}

/* --- account management functions --- */
PAM_EXTERN
int pam_sm_acct_mgmt(pam_handle_t* pamh, int flags, int argc, const char** argv)
{
    return PAM_SUCCESS;
}

/* --- password management --- */
PAM_EXTERN
int pam_sm_chauthtok(pam_handle_t* pamh, int flags, int argc, const char** argv)
{
    return PAM_IGNORE;
}

/* --- session management --- */
PAM_EXTERN
int pam_sm_open_session(pam_handle_t* pamh, int flags, int argc, const char** argv)
{
    return PAM_SUCCESS;
}

//------------------------------------------------------------------------------
PAM_EXTERN
int pam_sm_close_session(pam_handle_t* pamh, int flags, int argc, const char** argv)
{
    return PAM_SUCCESS;
}

