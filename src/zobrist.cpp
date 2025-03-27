#include <iomanip>
#include <vector>
#include <cstdint>
#include "piece.h"
#include "pos.h"
#include "square.h"
#include "zobrist.h"
using namespace std;

namespace zob {

// 12 * 64 + 8 + 4 + 1
static constexpr u64 keys[781] = {
    // piece square
    
    0x9d39247e33776d41ull, 0x2af7398005aaa5c7ull, 0x44db015024623547ull, 0x9c15f73e62a76ae2ull,
    0x75834465489c0c89ull, 0x3290ac3a203001bfull, 0x0fbbad1f61042279ull, 0xe83a908ff2fb60caull,
    0x0d7e765d58755c10ull, 0x1a083822ceafe02dull, 0x9605d5f0e25ec3b0ull, 0xd021ff5cd13a2ed5ull,
    0x40bdf15d4a672e32ull, 0x011355146fd56395ull, 0x5db4832046f3d9e5ull, 0x239f8b2d7ff719ccull,
    0x05d1a1ae85b49aa1ull, 0x679f848f6e8fc971ull, 0x7449bbff801fed0bull, 0x7d11cdb1c3b7adf0ull,
    0x82c7709e781eb7ccull, 0xf3218f1c9510786cull, 0x331478f3af51bbe6ull, 0x4bb38de5e7219443ull,
    0xaa649c6ebcfd50fcull, 0x8dbd98a352afd40bull, 0x87d2074b81d79217ull, 0x19f3c751d3e92ae1ull,
    0xb4ab30f062b19abfull, 0x7b0500ac42047ac4ull, 0xc9452ca81a09d85dull, 0x24aa6c514da27500ull,
    0x4c9f34427501b447ull, 0x14a68fd73c910841ull, 0xa71b9b83461cbd93ull, 0x03488b95b0f1850full,
    0x637b2b34ff93c040ull, 0x09d1bc9a3dd90a94ull, 0x3575668334a1dd3bull, 0x735e2b97a4c45a23ull,
    0x18727070f1bd400bull, 0x1fcbacd259bf02e7ull, 0xd310a7c2ce9b6555ull, 0xbf983fe0fe5d8244ull,
    0x9f74d14f7454a824ull, 0x51ebdc4ab9ba3035ull, 0x5c82c505db9ab0faull, 0xfcf7fe8a3430b241ull,
    0x3253a729b9ba3ddeull, 0x8c74c368081b3075ull, 0xb9bc6c87167c33e7ull, 0x7ef48f2b83024e20ull,
    0x11d505d4c351bd7full, 0x6568fca92c76a243ull, 0x4de0b0f40f32a7b8ull, 0x96d693460cc37e5dull,
    0x42e240cb63689f2full, 0x6d2bdcdae2919661ull, 0x42880b0236e4d951ull, 0x5f0f4a5898171bb6ull,
    0x39f890f579f92f88ull, 0x93c5b5f47356388bull, 0x63dc359d8d231b78ull, 0xec16ca8aea98ad76ull,
    0x5355f900c2a82dc7ull, 0x07fb9f855a997142ull, 0x5093417aa8a7ed5eull, 0x7bcbc38da25a7f3cull,
    0x19fc8a768cf4b6d4ull, 0x637a7780decfc0d9ull, 0x8249a47aee0e41f7ull, 0x79ad695501e7d1e8ull,
    0x14acbaf4777d5776ull, 0xf145b6beccdea195ull, 0xdabf2ac8201752fcull, 0x24c3c94df9c8d3f6ull,
    0xbb6e2924f03912eaull, 0x0ce26c0b95c980d9ull, 0xa49cd132bfbf7cc4ull, 0xe99d662af4243939ull,
    0x27e6ad7891165c3full, 0x8535f040b9744ff1ull, 0x54b3f4fa5f40d873ull, 0x72b12c32127fed2bull,
    0xee954d3c7b411f47ull, 0x9a85ac909a24eaa1ull, 0x70ac4cd9f04f21f5ull, 0xf9b89d3e99a075c2ull,
    0x87b3e2b2b5c907b1ull, 0xa366e5b8c54f48b8ull, 0xae4a9346cc3f7cf2ull, 0x1920c04d47267bbdull,
    0x87bf02c6b49e2ae9ull, 0x092237ac237f3859ull, 0xff07f64ef8ed14d0ull, 0x8de8dca9f03cc54eull,
    0x9c1633264db49c89ull, 0xb3f22c3d0b0b38edull, 0x390e5fb44d01144bull, 0x5bfea5b4712768e9ull,
    0x1e1032911fa78984ull, 0x9a74acb964e78cb3ull, 0x4f80f7a035dafb04ull, 0x6304d09a0b3738c4ull,
    0x2171e64683023a08ull, 0x5b9b63eb9ceff80cull, 0x506aacf489889342ull, 0x1881afc9a3a701d6ull,
    0x6503080440750644ull, 0xdfd395339cdbf4a7ull, 0xef927dbcf00c20f2ull, 0x7b32f7d1e03680ecull,
    0xb9fd7620e7316243ull, 0x05a7e8a57db91b77ull, 0xb5889c6e15630a75ull, 0x4a750a09ce9573f7ull,
    0xcf464cec899a2f8aull, 0xf538639ce705b824ull, 0x3c79a0ff5580ef7full, 0xede6c87f8477609dull,
    0x799e81f05bc93f31ull, 0x86536b8cf3428a8cull, 0x97d7374c60087b73ull, 0xa246637cff328532ull,
    0x043fcae60cc0eba0ull, 0x920e449535dd359eull, 0x70eb093b15b290ccull, 0x73a1921916591cbdull,
    0x56436c9fe1a1aa8dull, 0xefac4b70633b8f81ull, 0xbb215798d45df7afull, 0x45f20042f24f1768ull,
    0x930f80f4e8eb7462ull, 0xff6712ffcfd75ea1ull, 0xae623fd67468aa70ull, 0xdd2c5bc84bc8d8fcull,
    0x7eed120d54cf2dd9ull, 0x22fe545401165f1cull, 0xc91800e98fb99929ull, 0x808bd68e6ac10365ull,
    0xdec468145b7605f6ull, 0x1bede3a3aef53302ull, 0x43539603d6c55602ull, 0xaa969b5c691ccb7aull,
    0xa87832d392efee56ull, 0x65942c7b3c7e11aeull, 0xded2d633cad004f6ull, 0x21f08570f420e565ull,
    0xb415938d7da94e3cull, 0x91b859e59ecb6350ull, 0x10cff333e0ed804aull, 0x28aed140be0bb7ddull,
    0xc5cc1d89724fa456ull, 0x5648f680f11a2741ull, 0x2d255069f0b7dab3ull, 0x9bc5a38ef729abd4ull,
    0xef2f054308f6a2bcull, 0xaf2042f5cc5c2858ull, 0x480412bab7f5be2aull, 0xaef3af4a563dfe43ull,
    0x19afe59ae451497full, 0x52593803dff1e840ull, 0xf4f076e65f2ce6f0ull, 0x11379625747d5af3ull,
    0xbce5d2248682c115ull, 0x9da4243de836994full, 0x066f70b33fe09017ull, 0x4dc4de189b671a1cull,
    0x51039ab7712457c3ull, 0xc07a3f80c31fb4b4ull, 0xb46ee9c5e64a6e7cull, 0xb3819a42abe61c87ull,
    0x21a007933a522a20ull, 0x2df16f761598aa4full, 0x763c4a1371b368fdull, 0xf793c46702e086a0ull,
    0xd7288e012aeb8d31ull, 0xde336a2a4bc1c44bull, 0x0bf692b38d079f23ull, 0x2c604a7a177326b3ull,
    0x4850e73e03eb6064ull, 0xcfc447f1e53c8e1bull, 0xb05ca3f564268d99ull, 0x9ae182c8bc9474e8ull,
    0xa4fc4bd4fc5558caull, 0xe755178d58fc4e76ull, 0x69b97db1a4c03dfeull, 0xf9b5b7c4acc67c96ull,
    0xfc6a82d64b8655fbull, 0x9c684cb6c4d24417ull, 0x8ec97d2917456ed0ull, 0x6703df9d2924e97eull,
    0xc547f57e42a7444eull, 0x78e37644e7cad29eull, 0xfe9a44e9362f05faull, 0x08bd35cc38336615ull,
    0x9315e5eb3a129aceull, 0x94061b871e04df75ull, 0xdf1d9f9d784ba010ull, 0x3bba57b68871b59dull,
    0xd2b7adeeded1f73full, 0xf7a255d83bc373f8ull, 0xd7f4f2448c0ceb81ull, 0xd95be88cd210ffa7ull,
    0x336f52f8ff4728e7ull, 0xa74049dac312ac71ull, 0xa2f61bb6e437fdb5ull, 0x4f2a5cb07f6a35b3ull,
    0x87d380bda5bf7859ull, 0x16b9f7e06c453a21ull, 0x7ba2484c8a0fd54eull, 0xf3a678cad9a2e38cull,
    0x39b0bf7dde437ba2ull, 0xfcaf55c1bf8a4424ull, 0x18fcf680573fa594ull, 0x4c0563b89f495ac3ull,
    0x40e087931a00930dull, 0x8cffa9412eb642c1ull, 0x68ca39053261169full, 0x7a1ee967d27579e2ull,
    0x9d1d60e5076f5b6full, 0x3810e399b6f65ba2ull, 0x32095b6d4ab5f9b1ull, 0x35cab62109dd038aull,
    0xa90b24499fcfafb1ull, 0x77a225a07cc2c6bdull, 0x513e5e634c70e331ull, 0x4361c0ca3f692f12ull,
    0xd941aca44b20a45bull, 0x528f7c8602c5807bull, 0x52ab92beb9613989ull, 0x9d1dfa2efc557f73ull,
    0x722ff175f572c348ull, 0x1d1260a51107fe97ull, 0x7a249a57ec0c9ba2ull, 0x04208fe9e8f7f2d6ull,
    0x5a110c6058b920a0ull, 0x0cd9a497658a5698ull, 0x56fd23c8f9715a4cull, 0x284c847b9d887aaeull,
    0x04feabfbbdb619cbull, 0x742e1e651c60ba83ull, 0x9a9632e65904ad3cull, 0x881b82a13b51b9e2ull,
    0x506e6744cd974924ull, 0xb0183db56ffc6a79ull, 0x0ed9b915c66ed37eull, 0x5e11e86d5873d484ull,
    0xf678647e3519ac6eull, 0x1b85d488d0f20cc5ull, 0xdab9fe6525d89021ull, 0x0d151d86adb73615ull,
    0xa865a54edcc0f019ull, 0x93c42566aef98ffbull, 0x99e7afeabe000731ull, 0x48cbff086ddf285aull,
    0x7f9b6af1ebf78bafull, 0x58627e1a149bba21ull, 0x2cd16e2abd791e33ull, 0xd363eff5f0977996ull,
    0x0ce2a38c344a6eedull, 0x1a804aadb9cfa741ull, 0x907f30421d78c5deull, 0x501f65edb3034d07ull,
    0x37624ae5a48fa6e9ull, 0x957baf61700cff4eull, 0x3a6c27934e31188aull, 0xd49503536abca345ull,
    0x088e049589c432e0ull, 0xf943aee7febf21b8ull, 0x6c3b8e3e336139d3ull, 0x364f6ffa464ee52eull,
    0xd60f6dcedc314222ull, 0x56963b0dca418fc0ull, 0x16f50edf91e513afull, 0xef1955914b609f93ull,
    0x565601c0364e3228ull, 0xecb53939887e8175ull, 0xbac7a9a18531294bull, 0xb344c470397bba52ull,
    0x65d34954daf3cebdull, 0xb4b81b3fa97511e2ull, 0xb422061193d6f6a7ull, 0x071582401c38434dull,
    0x7a13f18bbedc4ff5ull, 0xbc4097b116c524d2ull, 0x59b97885e2f2ea28ull, 0x99170a5dc3115544ull,
    0x6f423357e7c6a9f9ull, 0x325928ee6e6f8794ull, 0xd0e4366228b03343ull, 0x565c31f7de89ea27ull,
    0x30f5611484119414ull, 0xd873db391292ed4full, 0x7bd94e1d8e17debcull, 0xc7d9f16864a76e94ull,
    0x947ae053ee56e63cull, 0xc8c93882f9475f5full, 0x3a9bf55ba91f81caull, 0xd9a11fbb3d9808e4ull,
    0x0fd22063edc29fcaull, 0xb3f256d8aca0b0b9ull, 0xb03031a8b4516e84ull, 0x35dd37d5871448afull,
    0xe9f6082b05542e4eull, 0xebfafa33d7254b59ull, 0x9255abb50d532280ull, 0xb9ab4ce57f2d34f3ull,
    0x693501d628297551ull, 0xc62c58f97dd949bfull, 0xcd454f8f19c5126aull, 0xbbe83f4ecc2bdecbull,
    0xdc842b7e2819e230ull, 0xba89142e007503b8ull, 0xa3bc941d0a5061cbull, 0xe9f6760e32cd8021ull,
    0x09c7e552bc76492full, 0x852f54934da55cc9ull, 0x8107fccf064fcf56ull, 0x098954d51fff6580ull,
    0x23b70edb1955c4bfull, 0xc330de426430f69dull, 0x4715ed43e8a45c0aull, 0xa8d7e4dab780a08dull,
    0x0572b974f03ce0bbull, 0xb57d2e985e1419c7ull, 0xe8d9ecbe2cf3d73full, 0x2fe4b17170e59750ull,
    0x11317ba87905e790ull, 0x7fbf21ec8a1f45ecull, 0x1725cabfcb045b00ull, 0x964e915cd5e2b207ull,
    0x3e2b8bcbf016d66dull, 0xbe7444e39328a0acull, 0xf85b2b4fbcde44b7ull, 0x49353fea39ba63b1ull,
    0x1dd01aafcd53486aull, 0x1fca8a92fd719f85ull, 0xfc7c95d827357afaull, 0x18a6a990c8b35ebdull,
    0xcccb7005c6b9c28dull, 0x3bdbb92c43b17f26ull, 0xaa70b5b4f89695a2ull, 0xe94c39a54a98307full,
    0xb7a0b174cff6f36eull, 0xd4dba84729af48adull, 0x2e18bc1ad9704a68ull, 0x2de0966daf2f8b1cull,
    0xb9c11d5b1e43a07eull, 0x64972d68dee33360ull, 0x94628d38d0c20584ull, 0xdbc0d2b6ab90a559ull,
    0xd2733c4335c6a72full, 0x7e75d99d94a70f4dull, 0x6ced1983376fa72bull, 0x97fcaacbf030bc24ull,
    0x7b77497b32503b12ull, 0x8547eddfb81ccb94ull, 0x79999cdff70902cbull, 0xcffe1939438e9b24ull,
    0x829626e3892d95d7ull, 0x92fae24291f2b3f1ull, 0x63e22c147b9c3403ull, 0xc678b6d860284a1cull,
    0x5873888850659ae7ull, 0x0981dcd296a8736dull, 0x9f65789a6509a440ull, 0x9ff38fed72e9052full,
    0xe479ee5b9930578cull, 0xe7f28ecd2d49eecdull, 0x56c074a581ea17feull, 0x5544f7d774b14aefull,
    0x7b3f0195fc6f290full, 0x12153635b2c0cf57ull, 0x7f5126dbba5e0ca7ull, 0x7a76956c3eafb413ull,
    0x3d5774a11d31ab39ull, 0x8a1b083821f40cb4ull, 0x7b4a38e32537df62ull, 0x950113646d1d6e03ull,
    0x4da8979a0041e8a9ull, 0x3bc36e078f7515d7ull, 0x5d0a12f27ad310d1ull, 0x7f9d1a2e1ebe1327ull,
    0xda3a361b1c5157b1ull, 0xdcdd7d20903d0c25ull, 0x36833336d068f707ull, 0xce68341f79893389ull,
    0xab9090168dd05f34ull, 0x43954b3252dc25e5ull, 0xb438c2b67f98e5e9ull, 0x10dcd78e3851a492ull,
    0xdbc27ab5447822bfull, 0x9b3cdb65f82ca382ull, 0xb67b7896167b4c84ull, 0xbfced1b0048eac50ull,
    0xa9119b60369ffebdull, 0x1fff7ac80904bf45ull, 0xac12fb171817eee7ull, 0xaf08da9177dda93dull,
    0x1b0cab936e65c744ull, 0xb559eb1d04e5e932ull, 0xc37b45b3f8d6f2baull, 0xc3a9dc228caac9e9ull,
    0xf3b8b6675a6507ffull, 0x9fc477de4ed681daull, 0x67378d8eccef96cbull, 0x6dd856d94d259236ull,
    0xa319ce15b0b4db31ull, 0x073973751f12dd5eull, 0x8a8e849eb32781a5ull, 0xe1925c71285279f5ull,
    0x74c04bf1790c0efeull, 0x4dda48153c94938aull, 0x9d266d6a1cc0542cull, 0x7440fb816508c4feull,
    0x13328503df48229full, 0xd6bf7baee43cac40ull, 0x4838d65f6ef6748full, 0x1e152328f3318deaull,
    0x8f8419a348f296bfull, 0x72c8834a5957b511ull, 0xd7a023a73260b45cull, 0x94ebc8abcfb56daeull,
    0x9fc10d0f989993e0ull, 0xde68a2355b93cae6ull, 0xa44cfe79ae538bbeull, 0x9d1d84fcce371425ull,
    0x51d2b1ab2ddfb636ull, 0x2fd7e4b9e72cd38cull, 0x65ca5b96b7552210ull, 0xdd69a0d8ab3b546dull,
    0x604d51b25fbf70e2ull, 0x73aa8a564fb7ac9eull, 0x1a8c1e992b941148ull, 0xaac40a2703d9bea0ull,
    0x764dbeae7fa4f3a6ull, 0x1e99b96e70a9be8bull, 0x2c5e9deb57ef4743ull, 0x3a938fee32d29981ull,
    0x26e6db8ffdf5adfeull, 0x469356c504ec9f9dull, 0xc8763c5b08d1908cull, 0x3f6c6af859d80055ull,
    0x7f7cc39420a3a545ull, 0x9bfb227ebdf4c5ceull, 0x89039d79d6fc5c5cull, 0x8fe88b57305e2ab6ull,
    0xa09e8c8c35ab96deull, 0xfa7e393983325753ull, 0xd6b6d0ecc617c699ull, 0xdfea21ea9e7557e3ull,
    0xb67c1fa481680af8ull, 0xca1e3785a9e724e5ull, 0x1cfc8bed0d681639ull, 0xd18d8549d140caeaull,
    0x4ed0fe7e9dc91335ull, 0xe4dbf0634473f5d2ull, 0x1761f93a44d5aefeull, 0x53898e4c3910da55ull,
    0x734de8181f6ec39aull, 0x2680b122baa28d97ull, 0x298af231c85bafabull, 0x7983eed3740847d5ull,
    0x66c1a2a1a60cd889ull, 0x9e17e49642a3e4c1ull, 0xedb454e7badc0805ull, 0x50b704cab602c329ull,
    0x4cc317fb9cddd023ull, 0x66b4835d9eafea22ull, 0x219b97e26ffc81bdull, 0x261e4e4c0a333a9dull,
    0x1fe2cca76517db90ull, 0xd7504dfa8816edbbull, 0xb9571fa04dc089c8ull, 0x1ddc0325259b27deull,
    0xcf3f4688801eb9aaull, 0xf4f5d05c10cab243ull, 0x38b6525c21a42b0eull, 0x36f60e2ba4fa6800ull,
    0xeb3593803173e0ceull, 0x9c4cd6257c5a3603ull, 0xaf0c317d32adaa8aull, 0x258e5a80c7204c4bull,
    0x8b889d624d44885dull, 0xf4d14597e660f855ull, 0xd4347f66ec8941c3ull, 0xe699ed85b0dfb40dull,
    0x2472f6207c2d0484ull, 0xc2a1e7b5b459aeb5ull, 0xab4f6451cc1d45ecull, 0x63767572ae3d6174ull,
    0xa59e0bd101731a28ull, 0x116d0016cb948f09ull, 0x2cf9c8ca052f6e9full, 0x0b090a7560a968e3ull,
    0xabeeddb2dde06ff1ull, 0x58efc10b06a2068dull, 0xc6e57a78fbd986e0ull, 0x2eab8ca63ce802d7ull,
    0x14a195640116f336ull, 0x7c0828dd624ec390ull, 0xd74bbe77e6116ac7ull, 0x804456af10f5fb53ull,
    0xebe9ea2adf4321c7ull, 0x03219a39ee587a30ull, 0x49787fef17af9924ull, 0xa1e9300cd8520548ull,
    0x5b45e522e4b1b4efull, 0xb49c3b3995091a36ull, 0xd4490ad526f14431ull, 0x12a8f216af9418c2ull,
    0x001f837cc7350524ull, 0x1877b51e57a764d5ull, 0xa2853b80f17f58eeull, 0x993e1de72d36d310ull,
    0xb3598080ce64a656ull, 0x252f59cf0d9f04bbull, 0xd23c8e176d113600ull, 0x1bda0492e7e4586eull,
    0x21e0bd5026c619bfull, 0x3b097adaf088f94eull, 0x8d14dedb30be846eull, 0xf95cffa23af5f6f4ull,
    0x3871700761b3f743ull, 0xca672b91e9e4fa16ull, 0x64c8e531bff53b55ull, 0x241260ed4ad1e87dull,
    0x106c09b972d2e822ull, 0x7fba195410e5ca30ull, 0x7884d9bc6cb569d8ull, 0x0647dfedcd894a29ull,
    0x63573ff03e224774ull, 0x4fc8e9560f91b123ull, 0x1db956e450275779ull, 0xb8d91274b9e9d4fbull,
    0xa2ebee47e2fbfce1ull, 0xd9f1f30ccd97fb09ull, 0xefed53d75fd64e6bull, 0x2e6d02c36017f67full,
    0xa9aa4d20db084e9bull, 0xb64be8d8b25396c1ull, 0x70cb6af7c2d5bcf0ull, 0x98f076a4f7a2322eull,
    0xbf84470805e69b5full, 0x94c3251f06f90cf3ull, 0x3e003e616a6591e9ull, 0xb925a6cd0421aff3ull,
    0x61bdd1307c66e300ull, 0xbf8d5108e27e0d48ull, 0x240ab57a8b888b20ull, 0xfc87614baf287e07ull,
    0xef02cdd06ffdb432ull, 0xa1082c0466df6c0aull, 0x8215e577001332c8ull, 0xd39bb9c3a48db6cfull,
    0x2738259634305c14ull, 0x61cf4f94c97df93dull, 0x1b6baca2ae4e125bull, 0x758f450c88572e0bull,
    0x959f587d507a8359ull, 0xb063e962e045f54dull, 0x60e8ed72c0dff5d1ull, 0x7b64978555326f9full,
    0xfd080d236da814baull, 0x8c90fd9b083f4558ull, 0x106f72fe81e2c590ull, 0x7976033a39f7d952ull,
    0xa4ec0132764ca04bull, 0x733ea705fae4fa77ull, 0xb4d8f77bc3e56167ull, 0x9e21f4f903b33fd9ull,
    0x9d765e419fb69f6dull, 0xd30c088ba61ea5efull, 0x5d94337fbfaf7f5bull, 0x1a4e4822eb4d7a59ull,
    0x6ffe73e81b637fb3ull, 0xddf957bc36d8b9caull, 0x64d0e29eea8838b3ull, 0x08dd9bdfd96b9f63ull,
    0x087e79e5a57d1d13ull, 0xe328e230e3e2b3fbull, 0x1c2559e30f0946beull, 0x720bf5f26f4d2eaaull,
    0xb0774d261cc609dbull, 0x443f64ec5a371195ull, 0x4112cf68649a260eull, 0xd813f2fab7f5c5caull,
    0x660d3257380841eeull, 0x59ac2c7873f910a3ull, 0xe846963877671a17ull, 0x93b633abfa3469f8ull,
    0xc0c0f5a60ef4cdcfull, 0xcaf21ecd4377b28cull, 0x57277707199b8175ull, 0x506c11b9d90e8b1dull,
    0xd83cc2687a19255full, 0x4a29c6465a314cd1ull, 0xed2df21216235097ull, 0xb5635c95ff7296e2ull,
    0x22af003ab672e811ull, 0x52e762596bf68235ull, 0x9aeba33ac6ecc6b0ull, 0x944f6de09134dfb6ull,
    0x6c47bec883a7de39ull, 0x6ad047c430a12104ull, 0xa5b1cfdba0ab4067ull, 0x7c45d833aff07862ull,
    0x5092ef950a16da0bull, 0x9338e69c052b8e7bull, 0x455a4b4cfe30e3f5ull, 0x6b02e63195ad0cf8ull,
    0x6b17b224bad6bf27ull, 0xd1e0ccd25bb9c169ull, 0xde0c89a556b9ae70ull, 0x50065e535a213cf6ull,
    0x9c1169fa2777b874ull, 0x78edefd694af1eedull, 0x6dc93d9526a50e68ull, 0xee97f453f06791edull,
    0x32ab0edb696703d3ull, 0x3a6853c7e70757a7ull, 0x31865ced6120f37dull, 0x67fef95d92607890ull,
    0x1f2b1d1f15f6dc9cull, 0xb69e38a8965c6b65ull, 0xaa9119ff184cccf4ull, 0xf43c732873f24c13ull,
    0xfb4a3d794a9a80d2ull, 0x3550c2321fd6109cull, 0x371f77e76bb8417eull, 0x6bfa9aae5ec05779ull,
    0xcd04f3ff001a4778ull, 0xe3273522064480caull, 0x9f91508bffcfc14aull, 0x049a7f41061a9e60ull,
    0xfcb6be43a9f2fe9bull, 0x08de8a1c7797da9bull, 0x8f9887e6078735a1ull, 0xb5b4071dbfc73a66ull,
    0x230e343dfba08d33ull, 0x43ed7f5a0fae657dull, 0x3a88a0fbbcb05c63ull, 0x21874b8b4d2dbc4full,
    0x1bdea12e35f6a8c9ull, 0x53c065c6c8e63528ull, 0xe34a1d250e7a8d6bull, 0xd6b04d3b7651dd7eull,
    0x5e90277e7cb39e2dull, 0x2c046f22062dc67dull, 0xb10bb459132d0a26ull, 0x3fa9ddfb67e2f199ull,
    0x0e09b88e1914f7afull, 0x10e8b35af3eeab37ull, 0x9eedeca8e272b933ull, 0xd4c718bc4ae8ae5full,
    0x81536d601170fc20ull, 0x91b534f885818a06ull, 0xec8177f83f900978ull, 0x190e714fada5156eull,
    0xb592bf39b0364963ull, 0x89c350c893ae7dc1ull, 0xac042e70f8b383f2ull, 0xb49b52e587a1ee60ull,
    0xfb152fe3ff26da89ull, 0x3e666e6f69ae2c15ull, 0x3b544ebe544c19f9ull, 0xe805a1e290cf2456ull,
    0x24b33c9d7ed25117ull, 0xe74733427b72f0c1ull, 0x0a804d18b7097475ull, 0x57e3306d881edb4full,
    0x4ae7d6a36eb5dbcbull, 0x2d8d5432157064c8ull, 0xd1e649de1e7f268bull, 0x8a328a1cedfe552cull,
    0x07a3aec79624c7daull, 0x84547ddc3e203c94ull, 0x990a98fd5071d263ull, 0x1a4ff12616eefc89ull,
    0xf6f7fd1431714200ull, 0x30c05b1ba332f41cull, 0x8d2636b81555a786ull, 0x46c9feb55d120902ull,
    0xccec0a73b49c9921ull, 0x4e9d2827355fc492ull, 0x19ebb029435dcb0full, 0x4659d2b743848a2cull,
    0x963ef2c96b33be31ull, 0x74f85198b05a2e7dull, 0x5a0f544dd2b1fb18ull, 0x03727073c2e134b1ull,
    0xc7f6aa2de59aea61ull, 0x352787baa0d7c22full, 0x9853eab63b5e0b35ull, 0xabbdcdd7ed5c0860ull,
    0xcf05daf5ac8d77b0ull, 0x49cad48cebf4a71eull, 0x7a4c10ec2158c4a6ull, 0xd9e92aa246bf719eull,
    0x13ae978d09fe5557ull, 0x730499af921549ffull, 0x4e4b705b92903ba4ull, 0xff577222c14f0a3aull,
    0x55b6344cf97aafaeull, 0xb862225b055b6960ull, 0xcac09afbddd2cdb4ull, 0xdaf8e9829fe96b5full,
    0xb5fdfc5d3132c498ull, 0x310cb380db6f7503ull, 0xe87fbb46217a360eull, 0x2102ae466ebb1148ull,
    0xf8549e1a3aa5e00dull, 0x07a69afdcc42261aull, 0xc4c118bfe78feaaeull, 0xf9f4892ed96bd438ull,
    0x1af3dbe25d8f45daull, 0xf5b4b0b0d2deeeb4ull, 0x962aceefa82e1c84ull, 0x046e3ecaaf453ce9ull,
    0xf05d129681949a4cull, 0x964781ce734b3c84ull, 0x9c2ed44081ce5fbdull, 0x522e23f3925e319eull,
    0x177e00f9fc32f791ull, 0x2bc60a63a6f3b3f2ull, 0x222bbfae61725606ull, 0x486289ddcc3d6780ull,
    0x7dc7785b8efdfc80ull, 0x8af38731c02ba980ull, 0x1fab64ea29a2ddf7ull, 0xe4d9429322cd065aull,
    0x9da058c67844f20cull, 0x24c0e332b70019b0ull, 0x233003b5a6cfe6adull, 0xd586bd01c5c217f6ull,
    0x5e5637885f29bc2bull, 0x7eba726d8c94094bull, 0x0a56a5f0bfe39272ull, 0xd79476a84ee20d06ull,
    0x9e4c1269baa4bf37ull, 0x17efee45b0dee640ull, 0x1d95b0a5fcf90bc6ull, 0x93cbe0b699c2585dull,
    0x65fa4f227a2b6d79ull, 0xd5f9e858292504d5ull, 0xc2b5a03f71471a6full, 0x59300222b4561e00ull,
    0xce2f8642ca0712dcull, 0x7ca9723fbb2e8988ull, 0x2785338347f2ba08ull, 0xc61bb3a141e50e8cull,
    0x150f361dab9dec26ull, 0x9f6a419d382595f4ull, 0x64a53dc924fe7ac9ull, 0x142de49fff7a7c3dull,
    0x0c335248857fa9e7ull, 0x0a9c32d5eae45305ull, 0xe6c42178c4bbb92eull, 0x71f1ce2490d20b07ull,
    0xf1bcc3d275afe51aull, 0xe728e8c83c334074ull, 0x96fbf83a12884624ull, 0x81a1549fd6573da5ull,
    0x5fa7867caf35e149ull, 0x56986e2ef3ed091bull, 0x917f1dd5f8886c61ull, 0xd20d8c88c8ffe65full,

    // castling
    
    0x31d71dce64b2c310ull, 0xf165b587df898190ull, 0xa57e6339dd2cf3a0ull, 0x1ef6e6dbb1961ec9ull,

    // en passant

    0x70cc73d90bc26e24ull, 0xe21a6b35df0c3ad7ull, 0x003a93d8b2806962ull, 0x1c99ded33cb890a1ull,
    0xcf3145de0add4289ull, 0xd0e4427a5514fb72ull, 0x77c621cc9fb3a483ull, 0x67a34dac4356550bull,
    
    // side
    
    0xf8d626aaaf278509ull
};

static constexpr int ZobristPieceIndex  =   0;
static constexpr int ZobristCastleIndex = 768;
static constexpr int ZobristEpIndex     = 772;
static constexpr int ZobristSideIndex   = 780;

static u64 ZobristCastleFast[16];

void init()
{
    for (int flags = 0; flags < 16; flags++) {
        u64 zobrist = 0;

        // modified to match polyglot

        if (flags & 1) zobrist ^= keys[ZobristCastleIndex + 0];
        if (flags & 4) zobrist ^= keys[ZobristCastleIndex + 1];
        if (flags & 2) zobrist ^= keys[ZobristCastleIndex + 2];
        if (flags & 8) zobrist ^= keys[ZobristCastleIndex + 3];

        ZobristCastleFast[flags] = zobrist;
    }
}

u64 piece(int piece, int sq)
{
    // modified to match polyglot

    int index = 64 * (piece ^ 1) + sq;

    return keys[ZobristPieceIndex + index];
}

u64 castle(u8 flags)
{
    return ZobristCastleFast[flags];
}

u64 ep(int sq)
{
    if (sq == square::None) return 0;

    int file = square::file(sq);

    return keys[ZobristEpIndex + file];
}

u64 side()
{
    return keys[ZobristSideIndex];
}

}
