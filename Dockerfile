FROM devkitpro/devkitppc:20210726
COPY --from=wiiuenv/wiiupluginsystem:20210417 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libkernel:20210407 /artifacts $DEVKITPRO

WORKDIR /app
CMD make -j$(nproc)
