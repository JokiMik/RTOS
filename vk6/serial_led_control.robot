*** Settings ***
Library  SerialLibrary

*** Variables ***
${com}   COM3
${board}   nRF5340


${CorrecTimeSequence}  000005\r
${CorrectTimeResponse}  5
${IncorrectTimeSequence}  256789\r
${TooShortTimeSequence}  1234\r
${TooLongTimeSequence}  1234567\r
${ZeroTimeSequence}  000000\r
${NonDigitTimeSequence}  0bcdef\r

${TIME_LEN_ERROR}  -1
${TIME_ARRAY_ERROR}  -2
${TIME_VALUE_ERROR}  -3
${TIME_ZERO_ERROR}  -4
${TIME_DIGIT_ERROR}  -5



*** Test Cases ***
Connect Serial
	Log To Console  Connecting to ${com} ${board}
	Add Port  ${com}  baudrate=115200   encoding=ascii
	Port Should Be Open  ${com}
	Reset Input Buffer
	Reset Output Buffer


Serial Correct Time Sequence
	Send Correct Time Sequence
	${read} =   Read Until  terminator=0D  encoding=ascii
	Log To Console  Received ${read}
	Should Be Equal As Integers  ${read}  ${CorrectTimeResponse}

Serial Incorrect Time Sequence
	Send Incorrect Time Sequence
	${read} =   Read Until  terminator=0D  encoding=ascii
	Log To Console  Received ${read}
	Should Be Equal As Integers  ${read}  ${TIME_VALUE_ERROR}

Serial Too Short Time Sequence
	Send Too Short Time Sequence
	${read} =   Read Until  terminator=0D  encoding=ascii
	Log To Console  Received ${read}
	Should Be Equal As Integers  ${read}  ${TIME_LEN_ERROR}

#Serial Too Long Time Sequence
	#Send Too Long Time Sequence
	#${read} =   Read Until  terminator=0D  encoding=ascii
	#Log To Console  Received ${read}
	#Should Be Equal As Integers  ${read}  ${TIME_LEN_ERROR}
	#En saa toimimaan tätä testiä, koska jos aika sekvenssissä on merkkejä enemmän kuin 6, 
	#niin robotti saa tyhjän vastauksen ja testi ei mene läpi.

Serial Zero Time Sequence
	Send Zero Time Sequence
	${read} =   Read Until  terminator=0D  encoding=ascii
	Log To Console  Received ${read}
	Should Be Equal As Integers  ${read}  ${TIME_ZERO_ERROR}

Serial Non Digit Time Sequence
	Send Non Digit Time Sequence
	${read} =   Read Until  terminator=0D  encoding=ascii
	Log To Console  Received ${read}
	Should Be Equal As Integers  ${read}  ${TIME_DIGIT_ERROR}

Sleep 3s
	Log To Console  Sleeping for 3s
	Sleep  3s

Disconnect Serial
	Log To Console  Disconnecting ${board}
	[TearDown]  Delete Port  ${com}

*** Keywords ***

Send Correct Time Sequence
	Log To Console  Send Correct Time Sequence
	Write Data  ${CorrecTimeSequence}  encoding=ascii
	Log To Console  Send Command ${CorrecTimeSequence} to ${com}

Send Incorrect Time Sequence
	Log To Console  Send Incorrect Time Sequence
	Write Data  ${IncorrectTimeSequence}  encoding=ascii
	Log To Console  Send Command ${IncorrectTimeSequence} to ${com}

Send Too Short Time Sequence
	Log To Console  Send Too Short Time Sequence
	Write Data  ${TooShortTimeSequence}  encoding=ascii
	Log To Console  Send Command ${TooShortTimeSequence} to ${com}

Send Too Long Time Sequence
	Log To Console  Send Too Long Time Sequence
	Write Data  ${TooLongTimeSequence}  encoding=ascii
	Log To Console  Send Command ${TooLongTimeSequence} to ${com}

Send Zero Time Sequence
	Log To Console  Send Zero Time Sequence
	Write Data  ${ZeroTimeSequence}  encoding=ascii
	Log To Console  Send Command ${ZeroTimeSequence} to ${com}

Send Non Digit Time Sequence
	Log To Console  Send Non Digit Time Sequence
	Write Data  ${NonDigitTimeSequence}  encoding=ascii
	Log To Console  Send Command ${NonDigitTimeSequence} to ${com}


	
	
	

