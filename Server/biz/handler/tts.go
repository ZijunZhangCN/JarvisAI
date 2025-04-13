package handler

import (
	"context"
	"encoding/base64"
	"esp32/biz/infra/tencent_cloud"
	"fmt"
	"github.com/bitly/go-simplejson"
	"github.com/cloudwego/hertz/pkg/app"
	"github.com/tencentcloud/tencentcloud-sdk-go/tencentcloud/common"
	"github.com/tencentcloud/tencentcloud-sdk-go/tencentcloud/common/errors"
	tts "github.com/tencentcloud/tencentcloud-sdk-go/tencentcloud/tts/v20190823"
	"io"
	"log"
	"net/http"
	"strconv"
	"strings"
	"time"
)

func TTS(ctx context.Context, c *app.RequestContext) {
	log.Println("start tts")
	inputText := c.Query("text")
	// 处理以https://开头的文本作为音频URL
	if strings.HasPrefix(inputText, "https://") {
		handleAudioURL(c, inputText)
	} else {
		handleTTSGeneration(c, inputText)
	}
}

func handleAudioURL(c *app.RequestContext, url string) {
	client := &http.Client{Timeout: 120 * time.Second}
	resp, err := client.Get(url)
	if err != nil {
		log.Printf("请求音频URL失败: %v", err)
		c.AbortWithError(http.StatusInternalServerError, err)
		return
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		log.Printf("音频URL返回非200状态码: %d", resp.StatusCode)
		c.AbortWithStatus(resp.StatusCode)
		return
	}

	contentLength := resp.Header.Get("Content-Length")
	c.Response.Header.Set("Content-Length", contentLength)
	c.Response.Header.Set("Content-Type", "audio/mpeg")

	// 高效流式传输（无论是否分块都适用）
	writer := c.Response.BodyWriter()
	buf := make([]byte, 32*2048)
	idx := 0
	for {
		n, err := resp.Body.Read(buf)
		if n > 0 {
			log.Printf("写入idx： %+v", idx)
			idx++
			if _, werr := writer.Write(buf[:n]); werr != nil {
				log.Printf("写入中断: %v", werr)
				break
			}
		}
		if err == io.EOF {
			log.Println("读取结束")
			break
		}
		if err != nil {
			log.Printf("读取错误: %v", err)
			break
		}
	}
	// --------------------------------------------

	c.Response.Header.Set("Connection", "close")
	fmt.Println("URL音频流传输完成")
}

func handleTTSGeneration(c *app.RequestContext, text string) {
	// 实例化一个请求对象,每个接口都会对应一个request对象
	request := tts.NewTextToVoiceRequest()

	request.Text = common.StringPtr(text)
	request.SessionId = common.StringPtr(fmt.Sprintf("%d", time.Now().UnixMilli()))
	voiceType := int64(501005)
	request.VoiceType = &voiceType
	//codec := "pcm"
	//request.Codec = &codec
	// 返回的resp是一个TextToVoiceResponse的实例，与请求对象对应
	response, err := tencent_cloud.TTSClient.TextToVoice(request)
	if _, ok := err.(*errors.TencentCloudSDKError); ok {
		fmt.Printf("An API error has returned: %s", err)
		_ = c.AbortWithError(-1, err)
		return
	}
	if err != nil {
		_ = c.AbortWithError(-1, err)
		panic(err)
	}
	// 输出json格式的字符串回包
	result := response.ToJsonString()
	sj, err := simplejson.NewJson([]byte(result))
	if err != nil {
		fmt.Printf("An API error has returned: %s", err)
		_ = c.AbortWithError(-1, err)
		return
	}

	// 1. 解析Base64音频数据
	audioBase64 := sj.GetPath("Response", "Audio").MustString()
	audioBytes, err := base64.StdEncoding.DecodeString(audioBase64) // 解码Base64
	if err != nil {
		fmt.Printf("An API error has returned: %s", err)
		_ = c.AbortWithError(-1, err)
		return
	}

	// 2. 设置二进制流响应头
	c.Response.Header.Set("Content-Type", "audio/mpeg")
	c.Response.Header.Set("Content-Length", strconv.Itoa(len(audioBytes)))
	fmt.Println("Content-Length: " + strconv.Itoa(len(audioBytes)))

	// 3. 分块流式写入数据（模拟实时生成效果）
	writer := c.Response.BodyWriter()
	chunkSize := 2048 // 与Arduino的BUFFER_SIZE对齐
	for offset := 0; offset < len(audioBytes); offset += chunkSize {
		end := offset + chunkSize
		if end > len(audioBytes) {
			end = len(audioBytes)
		}

		// 写入数据块并立即刷新
		if _, err := writer.Write(audioBytes[offset:end]); err != nil {
			log.Printf("写入中断: %v", err)
			break
		}
	}
	c.Response.Header.Set("Connection", "close")
	fmt.Println("finished")
}
