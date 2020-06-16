@echo off
bannertool.exe makebanner -i banner.png -a audio.wav -o banner.bnr
bannertool.exe makesmdh -s "Aeth Tra" -l "Battle Your Friends!" -p "dead girl games" -i icon.png  -o icon.icn
makerom -f cia -o aeth-tra.cia -DAPP_ENCRYPTED=false -rsf homebrew.rsf -target t -exefslogo -elf aeth-tra.elf -icon icon.icn -banner banner.bnr
makerom -f cci -o aeth-tra.3ds -DAPP_ENCRYPTED=true -rsf homebrew.rsf -target t -exefslogo -elf aeth-tra.elf -icon icon.icn -banner banner.bnr
echo Finished! 3DS and CIA have been built!
pause