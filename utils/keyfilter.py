

def keyfilter(key):
  if key == 'Shift':
    key = 'L Shift'
  elif key == 'Right shift':
    key = 'R Shift'
  elif key == 'Scroll lock':
    key = 'SrcLk'
  elif key == 'Insert':
    key = 'Ins'
  elif key == 'Delete':
    key = 'Del'
  elif key == 'Page up':
    key = 'PgUp'
  elif key == 'Page down':
    key = 'PgDn'
  elif key == '`':
    key = '~'
  elif key == 'Caps lock':
    key = 'Caps'
  elif key == 'Left windows':
    key = 'Win'
  elif key == 'Right windows':
    key = 'Win'
  elif key == 'Alt':
    key = 'L Alt'
  elif key == 'Right alt':
    key = 'R Alt'
  elif key == 'Ctrl':
    key = 'L Ctrl'
  elif key == 'Right ctrl':
    key = 'R Ctrl'
  elif key == ',':
    key = ', <'
  elif key == '.':
    key = '. >'
  elif key == '/':
    key = '/ ?'
  elif key == ';':
    key = '; :'
  elif key == '[':
    key = '[ {'
  elif key == ']':
    key = '] }'
  elif key == "Up":
    key = "↑"
  elif key == "Down":
    key = "↓"
  elif key == "Left":
    key = "←"
  elif key == "Right":
    key = "→"

  return key