processid=$(ps -C tna_disagg_switch -o pid h)
sudo gdb tna_disagg_switch -p $processid
