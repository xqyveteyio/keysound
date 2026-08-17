<template>
  <div class="about">
    <p>keysound重植版1.0</p>
  </div>
</template>


<style lang="less" scoped>
.about {
  flex: 1;
  min-width: 0;
  height: 100%;
  display: flex;
  flex-direction: column;
  justify-content: center;
  align-items: center;
  background-color: var(--bg);
  color: var(--fg);
  font-size: 20px;
  p{
    margin-bottom: 5px;
    &:last-child{
      margin-bottom: 0;
    }
  }
}
</style>
