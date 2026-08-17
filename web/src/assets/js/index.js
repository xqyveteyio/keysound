document.oncontextmenu = function(){
  event.returnValue = false;
}
// 或者直接返回整个事件
document.oncontextmenu = function(){
  return false;
}