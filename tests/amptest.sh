#!/bin/sh
# for the dummy amp values toggle between two likely values one each call
# Note FAULT is a string return
ampctl l FAULT l FAULT
# Note SWR is a float return
ampctl l SWR l SWR 
# All other values are integer
ampctl l NH l NH
ampctl l PF l PF
ampctl l PWRINPUT l PWRINPUT
ampctl l PWRFORWARD l PWRFORWARD
ampctl l PWRREFLECTED l PWRREFLECTED
ampctl l PWRPEAK l PWRPEAK
# Powerstat 0=Off, 1=On, 2=Standbyd, 4=Operate, 8=Unknown
ampctl \set_powerstat 0
ampctl \set_powerstat 1
ampctl \set_powerstat 2
ampctl \set_powerstat 4
ampctl \set_powerstat 8
# Sets/reads tuner frequenchy
ampctl F 14074000
ampctl f
