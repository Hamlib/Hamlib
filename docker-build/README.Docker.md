
# Build the hamlib-runtime from the repository root 

```
docker buildx build \
	--platform linux/amd64,linux/arm64 \
	--target hamlib-runtime \
	-t hamlib-runtime \
	-f docker-build/Dockerfile \
	.
```

# Interactively, develop with the hamlib-base-image


## Support the linux/amd64 and linux/arm64 platforms
```
docker buildx build \
	--platform linux/amd64,linux/arm64 \
	--target hamlib-base-image \
	-t hamlib-base-image \
	-f docker-build/Dockerfile \
	.
```

## Develop on the linux/amd64 hamlib-base-image

```
docker run \
	-it \
	--rm \
	--platform linux/amd64 \
	-v $(pwd):/wip \
	-w /wip \
	-u $(id -u):$(id -g) \
	hamlib-base-image bash
```

...or the linux/arm64 hamlib-base-image

```
docker run \
	-it \
	--rm \
	--platform linux/arm64 \
	-v $(pwd):/wip \
	-w /wip \
	-u $(id -u):$(id -g) \
	hamlib-base-image bash
```

# Run the linux/amd64 hamlib-runtime

```
docker run \
	-it \
	--rm \
	--platform linux/amd64 \
	hamlib-runtime bash
```

 ...or the linux/arm64 hamlib-runtime

```
docker run \
	-it \
	--rm \
	--platform linux/arm64 \
	hamlib-runtime bash
```

