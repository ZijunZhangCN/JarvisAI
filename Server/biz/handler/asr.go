package handler

import (
	"context"
	"esp32/biz/infra/tencent_cloud"
	"github.com/cloudwego/hertz/pkg/app"
	"github.com/cloudwego/hertz/pkg/protocol/consts"
	"github.com/tencentcloud/tencentcloud-speech-sdk-go/asr"
	"log"
)

const (
	EngineType = "16k_zh"
)

type ASRRequest struct {
	Data []byte `json:"data"`
}

type ASRResponse struct {
	Text string `json:"text"`
}

func ASR(ctx context.Context, c *app.RequestContext) {
	log.Println("start asr")
	var err error
	data, err := c.Body()
	if err != nil {
		_ = c.AbortWithError(-1, err)
		log.Printf("读取body失败: %v", err)
		return
	}

	recReq := &asr.FlashRecognitionRequest{
		EngineType:       EngineType,
		VoiceFormat:      "pcm",
		ConvertNumMode:   1,
		FirstChannelOnly: 1,
	}
	recResp, err := tencent_cloud.ASRClient.Recognize(recReq, data)
	if err != nil {
		log.Printf("语音识别失败: %v， %v", err, recResp)
		_ = c.AbortWithError(consts.StatusBadRequest, err)
		return
	}

	var text string
	for _, channelResult := range recResp.FlashResult {
		text += channelResult.Text
	}
	log.Printf("语音识别成功，结果: %s", text)
	resp := &ASRResponse{
		Text: text,
	}
	c.JSON(consts.StatusOK, resp)
}
