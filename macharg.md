# Mobile Action charger control description

Mobile Action USB cables based on Prolific **PL2303** USB-TTL chip (**MA-xxxxP**) has an **additional chip** to enable and disable charging voltage.  
This chip attached to **RX** and **TX** lines, but accesible only when communicating at **9600 bit/s** with **disabled DTR** line.

![Communication](/macomm.png?raw=true)

### The command sequence is (HEX):

```55 55 55 55 04 XX```

Where XX - command type:
    * 0 - disable charger
    * 1 - enable charger
    * 2 - get status
    * 3 - sync clock??? (answers with ```55``` x10)
    * 4 - same as command 2
No answer for any other byte.

55 x4 - sync clock (0b01010101)?  
04 - command start?


### The answer sequence for commands 0,1,2,4 is (HEX):

```55 A5 01 XX```

Where XX - charger status:
    * 00 - charger disabled
    * FF - charger enabled

55 - sync clock (0b01010101)?  
A5 - symmetric sync clock (0b10100101)???  
01 - status start?

![Raw communication](/maraw.png?raw=true)
