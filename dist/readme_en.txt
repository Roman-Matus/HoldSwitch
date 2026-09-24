HoldSwitch — switch the input language with a long key press
=============================================================
© 2026 .NoxCode · https://github.com/Roman-Matus/HoldSwitch

What it does
------------
To switch the input language, hold any letter, digit or symbol key a
little longer than usual, about half a second. Languages cycle in
order, for example: English → Russian → English.

Held letter keys no longer repeat ("aaaaaaa").

After a switch, a small blue label with the new language (EN, RU, etc.)
appears for a second. By default it shows in the bottom-right corner of
the screen; in the settings you can make it small and show it right
above the text cursor, where you are typing.

The usual shortcuts, Alt+Shift and Ctrl+Shift, keep working as before.


How to start
------------
1. Run HoldSwitch.exe. No installation is needed.

2. Windows may show "Windows protected your PC". This happens with any
   program that has no digital signature. Click "More info", then
   "Run anyway".

3. A blue "Яz" icon appears at the bottom right, next to the clock. If
   you don't see it, click the "^" arrow next to the clock.

To start the program automatically with the computer, click the icon
and check "Start with Windows".


How to update
-------------
1. Click the "Яz" icon and choose "Exit".
2. Replace HoldSwitch.exe with the new file.
3. Run it again. Your settings are kept.

While the program is running, Windows won't let you replace its file:
copying fails with an error such as "operation not permitted" or "file
in use". If the icon is gone but the error remains, end HoldSwitch.exe
in Task Manager (Ctrl+Shift+Esc).


Settings
--------
Click the "Яz" icon to open the menu.

  Enabled
      Uncheck to pause the program, for example while playing a game.
      The icon turns grey.

  On long press: what happens to the character itself.
      A — only switch the language (default).
          Nothing is typed; only the language changes.
      B — switch and type in the new language.
          Hold "f" in English: the language becomes Russian and "а"
          is typed. Handy when you notice the wrong language mid-word.
      C — type at once; on hold, erase and switch.
          The character appears immediately; if you keep holding the
          key, it is erased and the language switches.

  Hold time
      How long to hold a key before the language switches: 300 to
      800 ms (1000 ms = 1 second). If the language switches by accident
      while typing, choose a longer time. If waiting feels too long,
      choose a shorter one.

  Space bar switches too
      When checked, a long press on the space bar also switches the
      language.

  Language indicator: where to show the label with the new language.
      Bottom-right corner of the screen (default).
      Small, above the text cursor: above the blinking line where you
          type. If a program doesn't report where its cursor is, the
          label appears above the input field, and if that isn't known
          either, above the mouse pointer. Sometimes the label appears
          a fraction of a second later: the program asks the window
          again where the cursor is.
      Don't show.

  Start with Windows

  Язык / Language
      Menu and message language: same as Windows (default), Russian or
      English.

  About
      Version and author.

  Exit
      Close the program.

Settings are saved automatically.


Good to know
------------
- In modes A and B a character appears when you release the key, not
  when you press it: the program has to find out whether the press is
  a long one. In normal typing this is barely noticeable.

- Backspace, arrows, Delete and other editing keys still repeat when
  held.

- Shortcuts with Ctrl, Alt and Win work as usual and never switch the
  language. For example, holding Ctrl+Z still repeats Undo.

- The program doesn't work in windows opened "as administrator", such
  as Task Manager. Switch the language the usual way there.

- In full-screen games and remote desktop sessions it's best to pause
  the program: uncheck "Enabled".

- To find the text cursor, the program uses Windows accessibility
  features, the same ones screen readers use. In response, Chrome and
  Edge turn on their accessibility support. On a slow computer this can
  make the browser a little slower; if you notice it, choose the label
  in the bottom-right corner instead.


How to remove
-------------
1. Click the "Яz" icon and uncheck "Start with Windows".
2. Choose "Exit".
3. Delete HoldSwitch.exe.
4. To remove the settings too, press Win+R, enter
   %APPDATA%\HoldSwitch and delete that folder.


Author and rights
-----------------
HoldSwitch is made by .NoxCode.
© 2026 .NoxCode. Released under the MIT License: you may freely use,
copy and modify it as long as the copyright notice is kept. The full
text is in LICENSE.txt.

Source code: https://github.com/Roman-Matus/HoldSwitch


If something doesn't work right
-------------------------------
Tell us which program it happened in (Notepad, a browser, Word,
Telegram, Command Prompt…), which mode was selected (A, B or C) and
what exactly happened.

If the label doesn't appear above the text cursor, attach the file
%APPDATA%\HoldSwitch\caret.log (press Win+R, paste the path, press
Enter) and describe where the label was. The log is kept only when the
label is set to "above the text cursor": for each switch it records how
the program looked for the cursor, in which program (program name and
window type) and which coordinates it got. What you type is never
written there. The file is limited to 256 KB.
