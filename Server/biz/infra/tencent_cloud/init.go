package tencent_cloud

import (
	"github.com/tencentcloud/tencentcloud-sdk-go/tencentcloud/common"
	"github.com/tencentcloud/tencentcloud-sdk-go/tencentcloud/common/profile"
	tts "github.com/tencentcloud/tencentcloud-sdk-go/tencentcloud/tts/v20190823"
	"github.com/tencentcloud/tencentcloud-speech-sdk-go/asr"
	common2 "github.com/tencentcloud/tencentcloud-speech-sdk-go/common"
)

var (
	TTSClient *tts.Client
	ASRClient *asr.FlashRecognizer
)

func Init() {
	var err error
	SK := "" // TODO: 腾讯云SK
	AK := "" // TODO: 腾讯云AK
	credential := common.NewCredential(
		SK,
		AK,
	)
	// 实例化一个client选项，可选的，没有特殊需求可以跳过
	cpf := profile.NewClientProfile()
	cpf.HttpProfile.Endpoint = "tts.tencentcloudapi.com"
	// 实例化要请求产品的client对象,clientProfile是可选的
	TTSClient, err = tts.NewClient(credential, "", cpf)
	if err != nil {
		panic(err)
	}

	appID := "" // TODO：腾讯云APPID
	credential2 := common2.NewCredential(SK, AK)
	ASRClient = asr.NewFlashRecognizer(appID, credential2)
}
