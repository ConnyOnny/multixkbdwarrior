#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xutil.h>
#include <unistd.h>
#include <limits.h>
#include <string>
#include <string.h>
#include <vector>
#include <assert.h>
#include <map>
#include <iostream>
#include <set>
#include <sstream>

class Renderable {
public:
	virtual void render (Display*, Window&, GC&) = 0;
};

class TextRenderable : public Renderable {
public:
	int x, y; // position
	int color; // text color
	std::string text; // what to render
	//bool centered; // render centered // TODO

	TextRenderable (int posx, int posy, int textcolor, const char * message)
	 : x(posx), y(posy), color(textcolor), text(message) {}

	virtual void render (Display * dpy, Window & w, GC & gc) {
		XSetForeground(dpy, gc, color);
		XDrawString(dpy, w, gc, x, y, text.c_str(), text.length());
	}
};

class Player {
private:
	std::string name;
	bool isReady;
	TextRenderable * textField;
	int indexToType;
	int score;

public:
	Player () : name(), isReady(false), indexToType(0), score(0) {}
	int getScore () {
		return score;
	}
	void associateTextField (TextRenderable * t) {
		textField = t;
	}
	void nameInput (const char * input) {
		if (isReady)
			return;
		if (strlen(input) == 1) {
			textField->text.append(input);
		} else if (strcmp (input, "Return") == 0) {
			name = textField->text;
			isReady = true;
			textField->text.append(" READY");
		} else if (strcmp (input, "BackSpace") == 0 && name.length() > 0) {
			textField->text.erase (textField->text.length()-1);
		}
	}
	// returns if player has just finished the word
	bool input (const char * input, const std::string & currentWord) {
		if (strlen(input) == 1 && currentWord[indexToType] == input[0] && !isReady) {
			assert (textField != NULL);
			assert (indexToType >= 0);
			textField->text.push_back(input[0]);
			indexToType++;
			if (currentWord.length() == (unsigned int)indexToType) { // done
				indexToType = 0;
				textField->text.erase();
				isReady = true;
				return true;
			}
		}
		return false;
	}
	void clearText () {
		textField->text.erase();
		indexToType = 0;
	}
	void addScore (int toAdd) {
		assert (toAdd >= 0);
		score += toAdd;
	}
	void unsetReady () {
		isReady = false;
	}
	bool getReady () {
		return isReady;
	}
	const char * getName () {
		return name.c_str();
	}
};

void clearScreen (Display *d, Window& window, GC& gc, int color) {
	XSetForeground (d, gc, color);
	unsigned int width,height; // TODO: nicht jedes mal die query machen
	unsigned int dummyu; int dummyi; Window dummyw;
	XGetGeometry(d, window, &dummyw, &dummyi, &dummyi, &width, &height, &dummyu, &dummyu);
	XFillRectangle (d, window, gc, 0, 0, width, height);
}

void render (Display *d, Window& window, GC& gc, int bgcolor, std::vector < Renderable* > & toRender) {
	clearScreen (d, window, gc, bgcolor);
	for (size_t i=0; i<toRender.size(); i++) {
		toRender[i]->render(d, window, gc);
	}
	XFlush (d);
}

int getColorExact (Display *d, int r, int g, int b) {
	int screen = DefaultScreen(d);
	Colormap cmap = DefaultColormap(d,screen);

	if (r > 65535) r = 65535;
	if (g > 65535) g = 65535;
	if (b > 65535) b = 65535;

	XColor c;
	c.red = r; c.green = g; c.blue = b;

	if (XAllocColor(d, cmap, &c)) {
		if (c.red != r || c.green != g || c.blue != b) {
			fprintf (stderr, "(%d,%d,%d) -> (%d,%d,%d)\n", r, g, b, c.red, c.green, c.blue);
		}
		return c.pixel;
	} else {
		fprintf (stderr, "Warning: couldn't allocate color %d %d %d\n", r, g, b);
		return BlackPixel(d,screen);
	}
}

int getColor (Display *d, int r, int g, int b) {
	// 0->0, 255->65535
	return getColorExact (d, r*257, g*257, b*257);
}

std::string getNextWord (bool * isWord_return) {
	std::string ret;
	std::getline (std::cin, ret);
	*isWord_return = !std::cin.eof();
	return ret;
}

int main (int argc, char** argv) {
	/* Connect to the X server */
	Display *dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf (stderr, "XOpenDisplay returned no display to display what is to display. Aborting.\n");
		exit (1);
	}

	/* XInput Extension available? */
	int opcode, event, error;
	if (!XQueryExtension(dpy, "XInputExtension", &opcode, &event, &error)) {
	   printf("X Input extension not available.\n");
	   return -1;
	}

	/* Which version of XI2? We support 2.0 */
	int major = 2, minor = 0;
	if (XIQueryVersion(dpy, &major, &minor) == BadRequest) {
	  printf("XI2 not available. Server supports %d.%d\n", major, minor);
	  return -1;
	}
	
	printf ("system is sane.\n");
	
	// Get some colors
	int blackColor = BlackPixel(dpy, DefaultScreen(dpy));
	int whiteColor = WhitePixel(dpy, DefaultScreen(dpy));

	int nplayers=0;
	int maxplayers=4;
	TextRenderable playerTextFields [] = {
		TextRenderable(0, 200, getColor (dpy, 255, 220,  32), ""),
		TextRenderable(0, 300, getColor (dpy,  32, 128, 255), ""),
		TextRenderable(0, 400, getColor (dpy,  64, 255,   0), ""),
		TextRenderable(0, 500, getColor (dpy, 255,   0,  64), "")
	};

	// create window
	Window w = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 800, 600, 0, blackColor, blackColor);

	// which events we want?
	XSelectInput(dpy, w, StructureNotifyMask);

	// put window on the screen
	XMapWindow(dpy, w);

	// create graphics context
	GC gc = XCreateGC(dpy, w, 0, NULL);

	// wait for MapNotify
	while (true) {
		XEvent e;
		XNextEvent(dpy, &e);
		if (e.type == MapNotify)
			break;
	}

	// Now let's listen to keypress events
	XIEventMask eventmask;
	unsigned char mask [1] = { 0 }; // will change
	eventmask.deviceid = XIAllMasterDevices;
	eventmask.mask_len = sizeof (mask); // in bytes, for whatever reason...
	eventmask.mask = mask;
	XISetMask(mask, XI_KeyPress);
	XISelectEvents (dpy, w, &eventmask, 1);
	XSelectInput (dpy, w, ExposureMask);

	// setup player data structures
	std::map < int , Player* > kbd2player;
	std::vector< Player* > players;

	// setup rendering stuff
	std::vector< Renderable* > toRender;
	for (int i = 0; i < maxplayers; i++)
		toRender.push_back (playerTextFields+i);
	TextRenderable mainTextField (50, 50, whiteColor, "Please type your name and then hit Return.");
	toRender.push_back (&mainTextField);

	bool allReady = false;
	while (!allReady) {
		render(dpy, w, gc, blackColor, toRender);
		XEvent ev;
		XNextEvent(dpy, &ev);
		if (ev.xcookie.type == GenericEvent && ev.xcookie.extension == opcode
		    && ev.xcookie.evtype == XI_KeyPress && XGetEventData(dpy, &ev.xcookie)) {
		    XIDeviceEvent *d_ev = (XIDeviceEvent*) ev.xcookie.data;
		    KeyCode keycode = d_ev->detail;
		    int kbdid = d_ev->deviceid;
		    if (!(d_ev->flags & XIKeyRepeat)) {
				int keysyms_per_keycode;
				//KeySym *keysym = XGetKeyboardMapping (dpy, keycode, 1, &keysyms_per_keycode);
				KeySym keysym = XGetKeyboardMapping (dpy, keycode, 1, &keysyms_per_keycode)[0];
				Player * eventPlayer;
				if (kbd2player.count(kbdid) == 0) { // add player
					if (nplayers == maxplayers) {
						fprintf (stderr, "zu viele Spieler, TODO: gescheites error handling. Bye ;-)\n");
						exit (1);
					}
					eventPlayer = new Player ();
					eventPlayer->associateTextField (playerTextFields+nplayers);
					nplayers++;
					players.push_back(eventPlayer);
					kbd2player[kbdid] = eventPlayer;
				} else { // player already there
					eventPlayer = kbd2player[kbdid];
				}
				//debug out: mainTextField.text.append (XKeysymToString (keysym[0]));
				eventPlayer->nameInput (XKeysymToString (keysym));
			}

			XFreeEventData(dpy, &ev.xcookie);
			if (keycode == 9) {// Escape Key
				printf ("you escaped successfully.\n");
				exit(0);
			}
		} else if (ev.type == Expose) {
			printf ("exposeevent\n");
		} else {
			printf ("other event\n");
			//printf ("else\n");
		}
		if (!players.empty()) {
			allReady = true;
			for (size_t i=0; i<players.size() && allReady; i++) {
				allReady &= players[i]->getReady();
			}
		}
	}

	for (size_t i=0; i<players.size() && allReady; i++) {
		players[i]->clearText();
		players[i]->unsetReady();
	} // readyflag will now be used for if they are already done with a word

	int notreadyplayers = 0;
	std::string theWord;

	while (true) {
		assert (notreadyplayers >= 0);
		if (notreadyplayers == 0 || (notreadyplayers==1 && players.size()>1)) {
			bool gotWord;
			theWord = getNextWord(&gotWord);
			if (!gotWord)
				break;
			mainTextField.text = theWord;
			notreadyplayers = players.size();
			for (size_t i=0; i<players.size() && allReady; i++) {
				players[i]->unsetReady();
				players[i]->clearText();
				printf ("cleared text\n");
			}
		}
		render(dpy, w, gc, blackColor, toRender);
		XEvent ev;
		XNextEvent(dpy, &ev);
		if (ev.xcookie.type == GenericEvent && ev.xcookie.extension == opcode
		    && ev.xcookie.evtype == XI_KeyPress && XGetEventData(dpy, &ev.xcookie)) {
		    XIDeviceEvent *d_ev = (XIDeviceEvent*) ev.xcookie.data;
		    KeyCode keycode = d_ev->detail;
		    int kbdid = d_ev->deviceid;
		    if (!(d_ev->flags & XIKeyRepeat)) {
				printf ("got key event\n");
				int keysyms_per_keycode;
				//KeySym *keysym = XGetKeyboardMapping (dpy, keycode, 1, &keysyms_per_keycode);
				KeySym keysym = XGetKeyboardMapping (dpy, keycode, 1, &keysyms_per_keycode)[0];
				Player * eventPlayer;
				if (kbd2player.count(kbdid) == 0) { // add player
					printf ("you can't join in a match\n");
					exit (1);
					/*if (nplayers == maxplayers) {
						fprintf (stderr, "zu viele Spieler, TODO: gescheites error handling. Bye ;-)\n");
						exit (1);
					}
					eventPlayer = new Player ();
					eventPlayer->associateTextField (playerTextFields+nplayers);
					nplayers++;
					players.push_back(eventPlayer);
					kbd2player[srcid] = eventPlayer; */
				} else { // player already there
					eventPlayer = kbd2player[kbdid];
				}
				//debug out: mainTextField.text.append (XKeysymToString (keysym[0]));
				bool pfinished = eventPlayer->input (XKeysymToString (keysym), theWord);
				if (pfinished) {
					notreadyplayers--;
					eventPlayer->addScore (notreadyplayers);
					printf ("score of %d added\n", notreadyplayers);
				}
			}

			XFreeEventData(dpy, &ev.xcookie);
			if (keycode == 9) {// Escape Key
				printf ("you escaped successfully.\n");
				exit(0);
			}
		} else if (ev.type == Expose) {
			printf ("exposeevent\n");
		} else {
			printf ("other event\n");
			//printf ("else\n");
		}
	}

	for (size_t i=0; i<players.size(); i++) {
		std::stringstream msg;
		msg << "player " << players[i]->getName() << " has " << players[i]->getScore() << " points." << std::endl;
		std::cout << msg.str();
		playerTextFields[i].text = msg.str();
	}
	render(dpy, w, gc, blackColor, toRender);

	sleep(5);

	return 0;
}
