package infra

import (
	"esp32/biz/infra/coze"
	"esp32/biz/infra/tencent_cloud"
)

func Init() {
	coze.Init()
	tencent_cloud.Init()
}
