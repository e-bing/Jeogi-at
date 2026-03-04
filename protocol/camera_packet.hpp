#ifndef CAMERA_PACKET_HPP
#define CAMERA_PACKET_HPP

#include <cstdint>

namespace CamProtocol {

// 패킷 시작을 알리는 고유 번호 (Magic Number)
const uint32_t MAGIC_COOKIE = 0xDEADBEEF;

// 패킷 헤더 구조체 (16바이트 고정)
#pragma pack(push, \
             1)  // 패딩 제거: 컴파일러가 임의로 바이트를 추가하지 못하게 함
struct PacketHeader {
  uint32_t magic;       // MAGIC_COOKIE 확인용
  uint32_t camera_id;   // 1: Hanwha, 2: Pi_01, 3: Pi_02 등
  uint32_t json_size;   // JSON 데이터 크기 (바이트 단위)
  uint32_t image_size;  // JPEG 바이너리 크기 (바이트 단위)
};
#pragma pack(pop)

}  // namespace CamProtocol

#endif  // CAMERA_PACKET_HPP