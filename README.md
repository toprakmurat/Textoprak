Textoprak
===
Personal text editor with a Command Line Interface (CLI) based on [kilo text editor by @antirez](https://github.com/antirez/kilo)

### Usage
`textoprak.cfg` is the default configuration file for textoprak. 
Tab stops and quit times (how many times to press CTRL-Q without saving changes) 
could be set using this config file. Default values are 8 for tab stop, 3 for quit times.

If you want to open an empty text editor: textoprak 

If you want to open an existing file: textoprak `filename`

### Keys

      CTRL-S: Save 
      CTRL-Q: Quit 
      CTRL-F: Incremental search with arrow keys

There are some changes I want to add over time, such as: 
- Implementing `CTRL-Z`, `CTRL-C`, `CTRL-V`, `CTRL-D` etc.
- Adding line numbers to the left of the screen
- Adding more detailed syntax highlighting features
- Supporting more programming languages

Textoprak has simple syntax highlighting features for C, (partly C++) and Python.

Special thanks to [snaptoken](https://viewsourcecode.org/snaptoken/) for his well structured tutorial.
