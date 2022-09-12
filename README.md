# console_tester
visual regression tester for desktop opengl
program must take test file path on command line
- and a --geometry 800x800+800+0 or equivalent commandline option to set window size and screen position
put paths to 2 or more exe versions in version_list.txt
put paths to one or more test_list.txt in test_suite.txt
run console_tester 
- adjust size position of console window so minimum height at bottom of screen
- answer question prompts:
-- enter integer index to exe for left and right
-- enter integer index of test suite to run
will pass test file path to both left and right exe, wait a few seconds for keyboard scoring or control key, then kill both programs and repeat with next test file
keyboard scoring - any CAPS key, suggestions: D for difference, A for one side aborts B for blackscreen
keyboard control - a few lower case keys, see code for latest: ESC to exit early, p for pause/resume toggle, n next (early advance), b back, f faster s slower
when program ends, it shows scoring on screen and in test_report.txt
Programmer then manually runs and investigates difference / problem / bug
