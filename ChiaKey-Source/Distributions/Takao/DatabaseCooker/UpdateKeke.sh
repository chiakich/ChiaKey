#!/bin/sh
curl http://kekephrase.appspot.com/endpoint?type=%2B > ../DataSource/Addendum/Keke-Addendum.txt
curl http://kekephrase.appspot.com/endpoint?type=%2Bbpmf > ../DataSource/Overrides/Keke-AddBpmf.txt
curl http://kekephrase.appspot.com/endpoint?type=%2Bpinyin > ../DataSource/Overrides/Keke-AddPinyin.txt
curl http://kekephrase.appspot.com/endpoint?type=%2B2 > ../DataSource/Overrides/Keke-AddBigram.txt
curl http://kekephrase.appspot.com/endpoint?type=- > ../DataSource/Overrides/Keke-Removals.txt
curl http://kekephrase.appspot.com/endpoint?type=-bpmf >> ../DataSource/Overrides/Keke-Removals.txt
curl http://kekephrase.appspot.com/endpoint?type=demote-lowest > ../DataSource/Overrides/Keke-ZDemoteLowest.txt
curl http://kekephrase.appspot.com/endpoint?type=promote-highest > ../DataSource/Overrides/Keke-ZPromoteHighest.txt
curl http://kekephrase.appspot.com/endpoint?type=ensure-order > ../DataSource/Overrides/Keke-ZZEnsureOrder.txt
