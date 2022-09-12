#define _CRT_SECURE_NO_WARNINGS
// License MIT or similar permissive. Enjoy, good luck.
// developer list: dug9 of freewrl.sf.net, 
// developed for MS Windows operating system regression testing of desktop graphics rendering programs
//   programs must take scene file path on command line
//   programs must take window size and screen placement on command line by named commandline parameters
// 2 versions of same program, or 2 different programs, show same scene side-by-side 
// if they don't look the same press 'D' or other CAPS key of your choice to mark test failed and see console report at end
// (lower case keys are command keys if needed, such as p = toggle pause, ESC = exit early but save report ... see below)
// need 2 input files in local console_tester.exe folder:
// version_list.txt - holds list of paths to various versions of graphical program .exe
// suite_list.txt - holds list of paths to test_lists.txt files
// and you put at least one test_list.txt usually in same folder as, or near, your test scenes
// all 3 file types have same format X ROL where X is a command:
// # comment
// P push path prefix, p pop path prefix 
// F scene file 
// R .bat batch file that takes 2 exe paths on command line
// E executable 
// S path to another (sub) test_list.txt
// T push dutch_time preference in milliseconds before scene is automatically assumed OK, t pop dutch time preference
//   (dutch time refers to dutch flower auctions, price clock starts counting down and first to put up their hand buys flower batch)
// C push show console 1, 0 hide, c pop console preference
// and ROL is rest of line, which is blank or different depending on the command:
// .. for commands E,S,P,F,R - ROL is a string path ../../../tests/test_list.txt or path to exe or .bat, or path prefix
// .. C 0 or 1 show console, some programs need it 1, some can do 0
// .. T int miliseconds ie 5000 (5 seconds)
// .. t,c,p - blank, pops CAPs same-letter state variable
// On running, you'll be asked to choose which exe from the list for left, then right, by int index, 
//   and which test suite from the list by index
// then when it starts showing scenes, you do keyboard UPPER CASE CAPS key hit to indicate a (particular type of) fail, 
//   or if looks good, do nothing and the scene will change automatically, leaveing  ' ' score indicating OK by default
// Program shows your scoring at the end on both the console screen and in test_report.txt file in console_tester folder

#include <stdio.h>
#include <memory.h>
#include <windows.h>
#include <conio.h>
#include <direct.h> // _getcwd
#include <math.h>
#include <urlmon.h>
// launches 2 side-by-side graphical program versions on same scene 
//   for human operator visual regression test comparison
// quits them automatically by exe name via killtask.exe
// 

//path handling - converts to unquoted foreslashes on reading,
// assumes ample non-const string buffer size so string can be modified in-place
// and on windows just before using converts to backslashes and quotes as needed
char* strreplacechr(char* str, char target, char replacement) {
	for (int jj = 0; jj < (int)strlen(str); jj++)
		if (str[jj] == target) str[jj] = replacement;
	return str;
}
char* back2foreslash(char* str) {
	return strreplacechr(str, '\\', '/');
}
char* fore2backslash(char* str) {
	return strreplacechr(str, '/', '\\');
}
char* append_slash_if_missing(char* str) {
	//doesn't realloc, assumes you have a large enough buffer for another slash
	if (str) {
		int len = (int)strlen(str);
		if (len && str[len - 1] != '/')
			strcat(str, "/");
	}
	return str;
}
char* path_concatonate(char* scene_path, char* file_path, char* file_name) {
	strcpy(scene_path, file_path);
	append_slash_if_missing(scene_path);
	strcat(scene_path, file_name);
	return scene_path;
}
char* string_wrap_quotes(char* str) {
	int len = (int)strlen(str);
	if (len) {
		if (str[len - 1] != '"') {
			str[len] = '"';
			str[len + 1] = (char)0;
			len++;
		}
		if (str[0] != '"') {
			for (int i = len + 1; i > 0; i--) str[i] = str[i - 1];
			str[0] = '"';
		}
	}
	return str;
}
char* string_unwrap_quotes(char* str) {
	int len = (int)strlen(str);
	if (len) {
		if (str[0] == '"') {
			for (int i = 0; i < len + 1; i++) str[i] = str[i + 1];
			len--;
		}
		if (str[len - 1] == '"') {
			str[len - 1] = (char)0;
			len--;
		}

	}
	return str;
}

char* string_trim_endl(char* str) {
	int len = (int)strlen(str);
	if (len) {
		if (str[len-1] == '\r' || str[len - 1] == '\n') {
			len--;
		}
		if (str[len - 1] == '\r' || str[len - 1] == '\n') {
			len--;
		}
		str[len] = (char)0;
	}
	return str;
}
void filepath_split_prefix_filename(char* filepath, char** prefix, char** filename) {
	//inserts char 0 where last slash is, and returns pointer to second half of split
	//if no slash found, returns null
	*filename = filepath;
	*prefix = NULL;
	char* c = strrchr(filepath, '/');
	if (c) {
		*c = (char)0;
		c++;
		*prefix = filepath;
		*filename = c;
	}
}
struct ttest;
struct tlist {
	int n;
	int nalloc;
	struct ttest* p;
};
struct ttest {
	int itype; // 0 = testfile, 1 = R run.bat 2 = test_list
	char file_path[1024];
	char file_name[1024];
	int show_console; //0= no show 1=show
	int dutch_time; //int milliseconds, how long to show a test to human before assuming its OK and going to next test
	char score; //' ' not tested/skipped, 'x' fail, other codes depends on user flagging
	struct tlist* test_list; //NULL for itype=0 test file, run.bat, test_list[ns] for test_suite. 
};
void realloc_if_necessary(struct tlist* tests) {
	if (tests->n >= tests->nalloc) {
		int new_nalloc = tests->nalloc ? tests->nalloc * 2 : 10;
		tests->p = realloc(tests->p, sizeof(struct ttest) * new_nalloc);
		tests->nalloc = new_nalloc;
	}
}
struct tlist* load_test_list(struct ttest* tsuite)
{
	char test_list_path[1024];
	path_concatonate(test_list_path, tsuite->file_path, tsuite->file_name);
	if (!strncmp("http", test_list_path, 4)){
		static int download_count = 0;
		char local_file[1024], local_label[10];
		// https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createdirectorya
		CreateDirectoryA("download", NULL);
		strcpy(local_file, "download/");
		strcat(local_file, tsuite->file_name);
		sprintf(local_label, "_%d", download_count);
		strcat(local_file, local_label);
		download_count++;
		// https://docs.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/ms775123(v=vs.85)k
		URLDownloadToFileA(NULL, test_list_path, local_file, 0, NULL);
		printf("downloading to %s\n",local_file);
		Sleep(2500);
		strcpy(test_list_path, local_file);
	}
#ifdef _WIN32
	fore2backslash(test_list_path);
#endif //_WIN32

	FILE *fp = fopen(test_list_path, "r+");
	if (!fp) {
		printf("FILE NOT FOUND:%s\n", test_list_path);
		return NULL;
	}
	// recursive depth of nested test_lists and/or path prefixes: 10
	char line[1024], prefix[10][1024], rol[1024], cm;
	int show_console[10], dutch_time[10], nshow_console, ndutch_time;
	show_console[0] = tsuite->show_console; nshow_console = 1;
	dutch_time[0] = tsuite->dutch_time; ndutch_time = 1;
	int nt, np;
	char* ret; // *file_path;
	char* filename, * filepath;

	np = nt = 0;
	struct tlist* tests = malloc(sizeof(struct tlist));
	struct ttest* test;
	tests->nalloc = 100;
	tests->p = malloc(sizeof(struct ttest) * tests->nalloc);
	tests->n = 0;
	strcpy(prefix[0], tsuite->file_path); np = 1;
	do {
		ret = fgets(line, 1024, fp);

		if (ret) {
			//sscanf(line, "%c %s", &cm, rol); //assumes path names have no blanks and not "" wrapped, please change if needed
			cm = line[0];
			strcpy(rol, "");
			if (strlen(line) > 2) {
				strcpy(rol, &line[2]);
				string_trim_endl(rol);
			}
			switch (cm) {
			case 'P':
				back2foreslash(rol);
				path_concatonate(prefix[np], prefix[np - 1], rol);
				np++;
				break;
			case 'p':
				np--;
				break;
			case 'R':
			case 'F':
				//should there be a loop over the stack of folder names?
				back2foreslash(rol);
				string_unwrap_quotes(rol);
				realloc_if_necessary(tests);
				test = &tests->p[tests->n];
				filepath_split_prefix_filename(rol, &filepath, &filename);
				if (filepath)
					path_concatonate(test->file_path, prefix[np - 1], filepath);
				else
					strcpy(test->file_path, prefix[np - 1]);
				strcpy(test->file_name, filename);
				test->score = 0;
				test->itype = cm == 'R'? 1 : 0;
				test->show_console = show_console[nshow_console - 1];
				test->dutch_time = dutch_time[ndutch_time - 1];
				test->test_list = NULL;
				tests->n++;
				break;
			case 'S':
				//sub test list, recurse
				back2foreslash(rol);
				string_unwrap_quotes(rol);
				realloc_if_necessary(tests);
				test = &tests->p[tests->n];
				filepath_split_prefix_filename(rol, &filepath, &filename);
				if (filepath)
					path_concatonate(test->file_path, prefix[np - 1], filepath);
				else
					strcpy(test->file_path, prefix[np - 1]);
				strcpy(test->file_name, filename);
				test->score = 0;
				test->itype = 2;
				test->show_console = show_console[nshow_console - 1];
				test->dutch_time = dutch_time[ndutch_time - 1];
				test->test_list = load_test_list(test);
				tests->n++;
				break;
			case 'C': 
				sscanf(rol, "%d", &show_console[nshow_console - 1]);
				nshow_console++;
				break;
			case 'c':
				nshow_console--;
				break;
			case 'T':
				sscanf(rol, "%d", &dutch_time[ndutch_time]);
				ndutch_time++;
				break;
			case 't':
				ndutch_time--;
				break;
			case '#':
				//comment line
				break;
			case 'E': //not handled in test_list.txt
				break;
			case 'O': //not handled yet in test_list.txt, but could replace or add to commandline options set in version_list.txt
			case 'o': 
				break;
			default:
				printf("unknown command %c\n", cm);
			}
		}
	} while (ret);
	fclose(fp);
	return tests;
}
struct progversion {
	char* path;
	char* progname;
	char* options;
	char* geomfmt;
	int yoffset;
	int show_console;
};
static int time_scale = 0;
int crawl_list(struct tlist *tests, HWND shell_hwnd, struct progversion left, struct progversion right){
	char line[1024];
	int done = 0;
	for (int i = 0; i < tests->n; i++)
	{
		struct ttest* test = &tests->p[i];
		if (test->itype == 2) {
			if(test->test_list)
				done = crawl_list(test->test_list,shell_hwnd,left,right);
		}
		else {
			// https://stackoverflow.com/questions/19154516/running-an-external-exe-from-win32-application 
			// system or shellExecute?
			// https://docs.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutea?redirectedfrom=MSDN 
			// shell execute doesn't share this console_tester's console, so can launch multiple
			int showConsole = test->show_console == 0 ? SW_HIDE : SW_SHOW;
			//showConsole = SW_SHOW;
			int dutchTime = test->dutch_time;
			char scene_path[1024];
			path_concatonate(scene_path, test->file_path, test->file_name);
			string_wrap_quotes(scene_path);
#ifdef _WIN32
			fore2backslash(scene_path);
#endif //_WIN32
			strcpy(line, scene_path);
			int ipause = 0;
			int irelease_foreground = 0; // below in loop we keep setting this console window as foreground, but some configurations need to move the test windows
			if (test->itype == 1) {
				//R run a .bat and pass left and right executables as commandline parameters
				//char currentdir[1024];
				ipause = 1; // give the .bat time to run and human time to fiddle with it
				strcpy(line, left.path);
				strcat(line, " ");
				strcat(line, right.path);
				//GetCurrentDirectoryA(1023, currentdir);
				//SetCurrentDirectoryA(test->file_path);
				ShellExecuteA(NULL, NULL, test->file_name, line, test->file_path, SW_SHOW);
				irelease_foreground = 1;
				//SetCurrentDirectoryA(currentdir);
			}
			else {
				int show_console; //program show console over-rides scene-specific show-console when needed
				//F run 2 freewrl side by side
				strcat(line, " ");
				//strcat(line, "--geometry 800x800+0+50 ");
				if (left.geomfmt) {
					char geomstr[1024];
					sprintf(geomstr, left.geomfmt, 800, 800, 0, left.yoffset);
					strcat(line, geomstr);
					strcat(line, " ");
				}
				strcat(line, left.options);
				//printf("left_line: %s ", line);
				show_console = left.show_console ? SW_SHOW : showConsole;
				//printf("C %d\n", show_console);
				//printf("left line %s\n", line);
				//ShellExecuteA(NULL, NULL, "C:\\Program Files\\SenseGraphics\\H3DViewer\\bin64\\H3DViewer.exe", line, NULL, show_console);
				ShellExecuteA(NULL, NULL, left.path, line, NULL, show_console); // SW_HIDE); //SW_SHOW);

				strcpy(line, scene_path);
				//strcat(line, " -g 800x800+860+0_1 --pin TT -J sm"); 
				// -g geometry widthxheight+offsetx+offsety_windowtitleindex
				// we need _windowtitleindex if killing windows with FindWindowA + SendMessageA(,WM_CLOSE)
				// we don't need _windowtitleindex if shellExecutA(.."taskkill.exe" "... WINDOTITLE eq freeWRL"
				strcat(line, " ");
				//strcat(line, "--geometry 800x800+860+50 ");
				if (right.geomfmt) {
					char geomstr[1024];
					sprintf(geomstr, right.geomfmt, 800, 800, 800, right.yoffset);
					strcat(line, geomstr);
					strcat(line, " ");
				}

				strcat(line, right.options);
				//printf("right_line: %s ", line);
				show_console = right.show_console ? SW_SHOW : showConsole;
				//printf("C %d\n", show_console);
				ShellExecuteA(NULL, NULL, right.path, line, NULL, show_console); // SW_HIDE); //SW_SHOW);
				Sleep(250);
			}
			SetForegroundWindow(shell_hwnd);
			static int once = 0;
			static char blankline[121];
			if (!once) {
				static int console_width = 118;
				memset(blankline, ' ',120);
				blankline[console_width] = (char)0;
				//blankline[console_width - 1] = ']';
				printf("keyboard n=next b=back q=quit test_list, esc=quit suite,\n   x=fail test p= toggle pause s=slower f=faster\n   Use CAPS for any code\n\n");
				once = 1;
			}
			test->score = ' '; //' ' default usually means OK, any CAPS letter, example 'D' different 'A' one of them aborted 'B' one of them blackscreened etc.
			printf("%s\r", blankline);
			printf("%c %s\r", test->score, scene_path);
			done = 0;
			int maxloop = dutchTime / 50;
			for (int k = 0; k < maxloop; k++) {
				Sleep((int)(50.0*pow(1.1,(double)time_scale)));
				// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow
				if(!(irelease_foreground && k < 10))
					SetForegroundWindow(shell_hwnd);
				//https://docs.microsoft.com/en-us/windows/win32/inputdev/keyboard-input-reference
				// https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/kbhit?view=msvc-170
				int ibreak = 0;
				if (_kbhit()) {
					int c = _getch();
					switch (c) {
					case 'n': ibreak = 1; break; //next, jump to next test scene
					case 'q': ibreak = 1; done = 1; break; //quit this test sublist 
					case (char)27: ibreak = 1; done = 2; break; //ESC key done early, save report
					case 'b': ibreak = 1;  i -= 2; break; // back: go back one scene
					case 'p': ipause = 1 - ipause; if (!ipause) ibreak = 1; break; // pause timed scene switch, or unpause if paused (example use case: phone rings in middle of testing)
					case 's': time_scale++; /*printf("%d ", (int)(50.0 * pow(1.1, (double)time_scale)));*/ break; //faster auto-change-scene
					case 'f': time_scale--; /*printf("%d ", (int)(50.0 * pow(1.1, (double)time_scale)));*/ break; //slower
					default:
						//user can use any char to code, and see code in test_report.txt, except the command codes above
						//suggestion: toggle caps and use any caps key for scene coding
						test->score = c;
						printf("%c %s\r", test->score, scene_path);
						if (c == 'X') ibreak = 1;
					}
				}
				if (ipause) k--;
				if (ibreak) break;
			}
			if (test->itype == 0) {
				//https://docs.microsoft.com/en-us/windows-server/administration/windows-commands/taskkill
				char killstr[1024];
				strcpy(killstr, "/im ");
				strcat(killstr, left.progname);
				//ShellExecuteA(NULL, NULL, "taskkill.exe", "/im freeWRL.exe", NULL, FALSE); // SW_HIDE); //SW_SHOW);
				ShellExecuteA(NULL, NULL, "taskkill.exe", killstr, NULL, FALSE); // SW_HIDE); //SW_SHOW);
				strcpy(killstr, "/im ");
				strcat(killstr, right.progname);
				ShellExecuteA(NULL, NULL, "taskkill.exe", killstr, NULL, FALSE); // SW_HIDE); //SW_SHOW);
				Sleep(100);
			}

		}
		if (done) break;
	}
	return done < 2 ? 0 : 2;
}
struct testcounts {
	int total;
	int viewed;
	int failed;
	int scored;
};
void show_results(struct tlist* tests, FILE *fout, struct testcounts *tc) {
	for (int i = 0; i < tests->n; i++) {
		struct ttest* test = &tests->p[i];
		if (test->itype == 2)
			show_results(test->test_list, fout, tc);
		else 
		{
			tc->total++;
			if (test->score != (char)0) //display just the ones human-viewed (tested), 'x' means failed blank ' ' means OK
			{
				char scene_path[1024];
				tc->viewed++;
				path_concatonate(scene_path, test->file_path, test->file_name);
				fprintf(fout, "%c %s\n", test->score, scene_path);
				if (test->score == 'x') tc->failed++;
				if (test->score != ' ') tc->scored++;
			}
		}
	}
}
// https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getlocaltime
// https://docs.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-systemtime
char* month[] = { " ","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
char* day[] = { "Sun","Mon","Tues","Wed","Thurs","Fri","Sat" };
void show_results_to_file(struct tlist* tests, float deltatime, FILE *fout)
{
	struct testcounts tc;
	memset(&tc, 0, sizeof(struct testcounts));
	show_results(tests,fout, &tc);
	SYSTEMTIME lt;
	GetLocalTime(&lt);
	fprintf(fout, "# %s, %s %d %4d, %02d:%02d\n", day[lt.wDayOfWeek], month[lt.wMonth], lt.wDay, lt.wYear, lt.wHour, lt.wMinute);
	fprintf(fout, "# elapsed time %f minutes\n", deltatime);
	fprintf(fout, "# viewed: %d\n", tc.viewed);
	fprintf(fout, "# scored: %d\n", tc.scored);
	fprintf(fout, "# failed: %d\n", tc.failed);
	fprintf(fout, "# total:  %d\n", tc.total);
}

int main() {
	// let user know the working directory as seen by this test exe, in case paths aren't properly relative-to
	char* bname;
	bname = _getcwd(NULL, 0);
	if (bname) printf("cwd: %s\n", bname);
	//SYSTEMTIME start_time,end_time;
	//GetSystemTime(&start_time);
	ULONGLONG starttime = GetTickCount64();
	char* version_list_file = "version_list.txt";
	char* suite_list_file = "suite_list.txt";
	//have user select 2 program versions
	FILE *fp = fopen(version_list_file, "r+");
	char* ret;
	struct progversion pversion[10], left, right;
	int yoffset[10];
	char cm, line[1024], rol[1024]; // left[1024], left_options[1024], right[1024], right_options[1024];
	char prefix[10][1024], versions[10][1024], options[10][1024], geomfmt[10][1024]; //eoptions[10][1024], 
	int np, nv, nop, ng, ny;
	nv = 0;
	strcpy(prefix[0], ""); np = 1;
	strcpy(options[0], ""); nop = 1;
	ny = 1;
	yoffset[0] = 0;
	int show_console = 0;
	ng = 0;
	do {
		ret = fgets(line, 1024, fp);

		if (ret) {
			//sscanf(line,"%c %s", &cm, rol); //rol == rest of line
			cm = line[0];
			strcpy(rol, "");
			if (strlen(line) > 2) {
				strcpy(rol, &line[2]);
				string_trim_endl(rol);
			}
			switch(cm){
			case 'P':
				back2foreslash(rol);
				string_unwrap_quotes(rol);
				path_concatonate(prefix[np], prefix[np - 1], rol);
				np++;
				break;
			case 'p':
				np--;
				break;
			case 'O':
				//commandline options (except for scenefile and --geometry)
				string_unwrap_quotes(rol);
				strcpy(options[nop],rol);
				nop++;
				break;
			case 'o':
				nop--;
				break;
			case 'G':
				//format string for window geometry example "-geometry %dx%d+%d+%d" with the 4 parameters filled in the program in the order width height xoffset yoffset
				string_unwrap_quotes(rol);
				strcpy(geomfmt[ng], rol);
				ng++;
				break;
			case 'g':
				ng--;
				break;
			case 'Y':
				//y offset for geometry, some apps need to be lower than 0 from the top to see the title bar
				sscanf(rol, "%d", &yoffset[ny]);
				ny++;
				break;
			case 'y':
				ny--;
				break;
			case 'C':
				show_console = 1;
				break;
			case 'c':
				show_console = 0;
				break;
			case 'E':
				back2foreslash(rol);
				string_unwrap_quotes(rol);
				path_concatonate(versions[nv], prefix[np - 1], rol);
				//strcpy(eoptions[nv], options[nop - 1]);
				pversion[nv].path = _strdup(versions[nv]); // prefix[np - 1];
				pversion[nv].progname = _strdup(rol);
				pversion[nv].yoffset = yoffset[ny-1];
				pversion[nv].options = _strdup(options[nop - 1]);
				pversion[nv].geomfmt = ng ? _strdup(geomfmt[ng - 1]) : NULL;
				pversion[nv].show_console = show_console;
				nv++;
				break;
			case '#':
				//comment line
				break;
			default:
				printf("unknown command %c\n", cm);
			}
		}
	} while (ret);
	fclose(fp);
	printf("program versions:\n");
	for(int i=0;i<nv;i++){
		printf("%d %s\n", i, versions[i]);
	}
	int ileft = 0;
	printf("Choice for left side (0-%d)[%d]:", nv - 1,ileft);
	//scanf("%s",line);
	fgets(line, 1024, stdin);
	if (strlen(line)) 
		sscanf(line, "%d", &ileft);
	//strcpy(left, versions[ileft]);
	//strcpy(left_options, eoptions[ileft]);
	left = pversion[ileft];
	int iright = 1;
	printf("Choice for right side (0-%d)[%d]:", nv - 1,iright);
	//scanf("%s", line);
	fgets(line, 1024, stdin);
	if (strlen(line)) 
		sscanf(line, "%d", &iright);
	//strcpy(right, versions[iright]);
	//strcpy(right_options, eoptions[iright]);
	right = pversion[iright];
#ifdef _WIN32
	fore2backslash(left.path);
	fore2backslash(right.path);
#endif //_WIN32

	//ask user to select test suite
	fp = fopen(suite_list_file, "r+");
	strcpy(prefix[0], ""); np = 1;
	int dutch_time = 5000;

	struct tlist* suites = malloc(sizeof(struct tlist));
	struct ttest *tsuite;
	suites->nalloc = 100;
	suites->p = malloc(sizeof(struct ttest) * suites->nalloc);
	suites->n = 0;
	strcpy(prefix[0], ""); np = 1;


	do {
		ret = fgets(line, 1024, fp);
		if (ret) {
			//sscanf(line, "%c %s", &cm, rol);
			cm = line[0];
			strcpy(rol, "");
			if (strlen(line) > 2) {
				strcpy(rol, &line[2]);
				string_trim_endl(rol);
			}
			switch (cm) {
			case 'P':
				back2foreslash(rol);
				string_unwrap_quotes(rol);
				path_concatonate(prefix[np], prefix[np - 1], rol);
				np++;
				break;
			case 'p':
				np--;
				break;
			case 'S':
				//sub test list
				back2foreslash(rol);
				string_unwrap_quotes(rol);
				realloc_if_necessary(suites);
				char * filename, * filepath;
				tsuite = &suites->p[suites->n];
				filepath_split_prefix_filename(rol, &filepath, &filename);
				if (filepath)
					path_concatonate(tsuite->file_path, prefix[np - 1], filepath);
				else
					strcpy(tsuite->file_path, prefix[np - 1]);
				strcpy(tsuite->file_name, filename);
				tsuite->show_console = show_console;
				tsuite->dutch_time = dutch_time;
				tsuite->itype = 0;
				suites->n++;
				break;
			case '#':
				//comment line
				break;
			case 'F': //not handled in suite_list.txt file
			case 'E':
				break;
			case 'C':
				sscanf(rol, "%d", &show_console); break;
			case 'c':
				show_console = 0; break;
			case 'T':
				sscanf(rol, "%d", &dutch_time); break;
			case 't':
				dutch_time = 5000; break;
			default:
				printf("unknown command %c\n", cm);
			}
		}
	} while (ret);
	fclose(fp);

	for (int i = 0; i < suites->n; i++) {
		struct ttest* tsuite = &suites->p[i];
		printf("%2d %s/%s\n", i, tsuite->file_path,tsuite->file_name);
	}
	int isuite = 0;
	printf("test suite (0-%d)[%d]:", suites->n - 1,isuite);
	fgets(line, 1024, stdin);
	if (strlen(line))
		sscanf(line, "%d", &isuite);
	tsuite = &suites->p[isuite];
	struct tlist *tests = load_test_list(tsuite);

	// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getshellwindow
	HWND shell_hwnd = GetConsoleWindow(); // GetShellWindow(); 
	//CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	int done = crawl_list(tests,shell_hwnd,left,right);

	//show results
	ULONGLONG endtime = GetTickCount64();
	float deltatime = (float)(endtime - starttime)/1000.0f/60.0f;
	show_results_to_file(tests, deltatime, stdout);
	FILE* fout = fopen("test_report.txt", "w+");
	show_results_to_file(tests, deltatime, fout);
	fclose(fout);
	printf("Done. Press Enter to exit:");
	fgets(line, 1024, stdin);

}