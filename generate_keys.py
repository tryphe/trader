import sys
import os.path

def add_quotes(a):
  return '"{0}"'.format(a)

MSG = "(press enter to leave blank for later, or CTRL-C to abort):"
KEYDEFS_FILE = "daemon/keydefs.h"

if not os.path.exists( KEYDEFS_FILE ):
  print( "NOTE: If you don't want to hardcode these values, leave them blank and you can use 'setkeyandsecret <key> <secret>' in the bot upon startup. Alternately, if you don't want to use this script, but still want to hardcode your keys, CTRL-C and 'cp daemon/keydefs.h.example daemon/keydefs.h' and paste your keys in there." )
  print( "Generating keydefs.h..." )

  polo_key = add_quotes( raw_input( "Enter Poloniex Key" + MSG ) )
  polo_secret = add_quotes( raw_input( "Enter Poloniex Secret" + MSG ) )
  trex_key = add_quotes( raw_input( "Enter Bittrex Key" + MSG ) )
  trex_secret = add_quotes( raw_input( "Enter Bittrex Secret" + MSG ) )
  bnc_key = add_quotes( raw_input( "Enter Binance Key" + MSG ) )
  bnc_secret = add_quotes( raw_input( "Enter Binance Secret" + MSG ) )

  file = open( KEYDEFS_FILE, "w" )
  file.write( "#ifndef KEYDEFS_H\n" )
  file.write( "#define KEYDEFS_H\n\n" )

  file.write( "#define POLONIEX_KEY " + polo_key + "\n" )
  file.write( "#define POLONIEX_SECRET " + polo_secret + "\n" )
  file.write( "#define BITTREX_KEY " + trex_key + "\n" )
  file.write( "#define BITTREX_SECRET " + trex_secret + "\n" )
  file.write( "#define BINANCE_KEY " + bnc_key + "\n" )
  file.write( "#define BINANCE_SECRET " + bnc_secret + "\n\n" )


  file.write( "#endif // KEYDEFS_H\n" )
  file.close();

else:
  print( "keydefs.h already exists, exiting.. (if you meant to overwrite it, remove/move the file and re-run this script.)" )
